#!/usr/bin/env python3
"""Minimal local HTTP server for download integration tests."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
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
            self.send_header("Location", "//127.0.0.1:18080/plain.txt")
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

        self.send_response(404)
        self.end_headers()

    def log_message(self, format, *args):
        return


def main():
    server = ThreadingHTTPServer(("127.0.0.1", 18080), Handler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
