#!/usr/bin/env python3
"""Minimal local HTTP server for download integration tests."""

from __future__ import annotations

import json
import os
import re
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SEGMENTED_SIZE = 512 * 1024
SEGMENTED_BODY = bytes((index % 256) for index in range(SEGMENTED_SIZE))

STATS_ENV = "AVAR_TEST_RANGE_STATS_PATH"
PORT_ENV = "AVAR_TEST_HTTP_PORT"

STATS_PATH = os.environ.get(
    STATS_ENV,
    os.path.join(os.environ.get("TEMP", tempfile.gettempdir()), "avar-range-stats.json"),
)
HTTP_PORT = int(os.environ.get(PORT_ENV, "18080"))

# Keep each range response open briefly so fast CI hosts still observe
# overlapping segment requests when the client opens parallel connections.
RANGE_STAT_HOLD_MS = int(os.environ.get("AVAR_TEST_RANGE_STAT_HOLD_MS", "75"))

_stats_lock = threading.Lock()
_active_range_requests = 0
_max_concurrent_range_requests = 0
_range_request_count = 0

# Tracks how many times each byte range of /flaky_segmented.bin has been
# requested so the first attempt per range can be failed deterministically.
_flaky_lock = threading.Lock()
_flaky_attempts: dict[tuple[int, int], int] = {}


def _write_stats_locked() -> None:
    payload = {
        "active": _active_range_requests,
        "max_concurrent": _max_concurrent_range_requests,
        "total_range_requests": _range_request_count,
    }
    with open(STATS_PATH, "w", encoding="ascii") as handle:
        json.dump(payload, handle, separators=(",", ":"))


def _reset_stats() -> None:
    global _active_range_requests, _max_concurrent_range_requests, _range_request_count
    with _stats_lock:
        _active_range_requests = 0
        _max_concurrent_range_requests = 0
        _range_request_count = 0
        _write_stats_locked()
    with _flaky_lock:
        _flaky_attempts.clear()


def _parse_range_header(value: str, total_size: int) -> tuple[int, int] | None:
    match = re.match(r"bytes=(\d+)-(\d*)", value.strip())
    if match is None:
        return None

    start = int(match.group(1))
    end_str = match.group(2)
    end = int(end_str) if end_str else total_size - 1

    if start >= total_size or end < start:
        return None

    end = min(end, total_size - 1)
    return start, end


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        if self.path == "/hls/playlist.m3u8":
            body = (
                b"#EXTM3U\n"
                b"#EXT-X-VERSION:3\n"
                b"#EXT-X-TARGETDURATION:1\n"
                b"#EXT-X-MEDIA-SEQUENCE:0\n"
                b"segment0.ts\n"
                b"segment1.ts\n"
            )
            self.send_response(200)
            self.send_header("Content-Type", "application/vnd.apple.mpegurl")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path.startswith("/hls/segment"):
            body = b"hls-segment-data"
            self.send_response(200)
            self.send_header("Content-Type", "video/mp2t")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path == "/plain.txt":
            body = b"hello world"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Disposition", 'attachment; filename="plain.txt"')
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path == "/redirect.bin":
            self.send_response(302)
            self.send_header("Location", f"//127.0.0.1:{HTTP_PORT}/plain.txt")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return

        if self.path == "/chunked.txt":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for chunk in (b"chunk", b"-ed"):
                data = chunk
                self.wfile.write(f"{len(data):X}\r\n".encode("ascii"))
                self.wfile.write(data)
                self.wfile.write(b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
            return

        if self.path == "/range_stats":
            with _stats_lock:
                payload = {
                    "active": _active_range_requests,
                    "max_concurrent": _max_concurrent_range_requests,
                    "total_range_requests": _range_request_count,
                }
            body = json.dumps(payload).encode("ascii")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path == "/test_reset":
            _reset_stats()
            self.send_response(204)
            self.end_headers()
            return

        if self.path.startswith("/flaky_segmented"):
            self._serve_flaky_segmented()
            return

        if self.path.startswith("/liesrange"):
            self._serve_lies_about_ranges()
            return

        if self.path.startswith("/noresume"):
            self._serve_no_resume()
            return

        if self.path.startswith("/segmented"):
            self._serve_segmented()
            return

        self.send_response(404)
        self.end_headers()

    def _serve_segmented(self) -> None:
        global _active_range_requests, _max_concurrent_range_requests, _range_request_count

        filename = self.path.split("?", 1)[0].lstrip("/")
        if filename == "":
            filename = "segmented.bin"

        range_header = self.headers.get("Range")
        if range_header is None:
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(SEGMENTED_SIZE))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
            self.end_headers()
            self.wfile.write(SEGMENTED_BODY)
            return

        parsed = _parse_range_header(range_header, SEGMENTED_SIZE)
        if parsed is None:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{SEGMENTED_SIZE}")
            self.end_headers()
            return

        start, end = parsed
        body = SEGMENTED_BODY[start : end + 1]

        with _stats_lock:
            _range_request_count += 1
            _active_range_requests += 1
            if _active_range_requests > _max_concurrent_range_requests:
                _max_concurrent_range_requests = _active_range_requests
            _write_stats_locked()

        try:
            if RANGE_STAT_HOLD_MS > 0:
                time.sleep(RANGE_STAT_HOLD_MS / 1000.0)
            self.send_response(206)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Content-Range", f"bytes {start}-{end}/{SEGMENTED_SIZE}")
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
            self.end_headers()
            self.wfile.write(body)
        finally:
            with _stats_lock:
                _active_range_requests -= 1
                _write_stats_locked()

    def _serve_lies_about_ranges(self) -> None:
        """Advertises range support but always answers 200 with the full body.

        Exercises the client's fallback from segmented to streaming mode when a
        server ignores Range requests instead of replying 206.
        """
        filename = self.path.split("?", 1)[0].lstrip("/") or "liesrange.bin"
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(SEGMENTED_SIZE))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
        self.end_headers()
        self.wfile.write(SEGMENTED_BODY)

    def _serve_no_resume(self) -> None:
        """Accepts an initial full download but rejects resume range requests."""
        filename = self.path.split("?", 1)[0].lstrip("/") or "noresume.bin"
        range_header = self.headers.get("Range")
        if range_header is not None:
            parsed = _parse_range_header(range_header, SEGMENTED_SIZE)
            if parsed is None or parsed[0] > 0:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{SEGMENTED_SIZE}")
                self.end_headers()
                return

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(SEGMENTED_SIZE))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
        self.end_headers()
        self.wfile.write(SEGMENTED_BODY)

    def _serve_flaky_segmented(self) -> None:
        """Like /segmented, but drops the connection mid-body on the first attempt
        for each range so the client must retry the segment independently."""
        filename = self.path.split("?", 1)[0].lstrip("/") or "flaky_segmented.bin"

        range_header = self.headers.get("Range")
        if range_header is None:
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(SEGMENTED_SIZE))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
            self.end_headers()
            self.wfile.write(SEGMENTED_BODY)
            return

        parsed = _parse_range_header(range_header, SEGMENTED_SIZE)
        if parsed is None:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{SEGMENTED_SIZE}")
            self.end_headers()
            return

        start, end = parsed
        body = SEGMENTED_BODY[start : end + 1]

        with _flaky_lock:
            attempt = _flaky_attempts.get((start, end), 0)
            _flaky_attempts[(start, end)] = attempt + 1

        self.send_response(206)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Range", f"bytes {start}-{end}/{SEGMENTED_SIZE}")
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
        self.end_headers()

        if attempt == 0 and len(body) > 1:
            # Send only part of the promised body, then drop the connection.
            self.wfile.write(body[: len(body) // 2])
            self.close_connection = True
            return

        self.wfile.write(body)

    def log_message(self, format, *args) -> None:
        return


def main() -> None:
    _reset_stats()
    server = ThreadingHTTPServer(("127.0.0.1", HTTP_PORT), Handler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
