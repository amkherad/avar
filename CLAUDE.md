# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Avar is a cross-platform download manager written in C23: a CLI + background daemon in `src/`, a React/Electron GUI in `gui/`, and browser extensions in `extensions/`. `AGENTS.md` defines directory-scoped skills and rules — follow it (notably: C skills never apply to `gui/`, and extensions must stay site-agnostic with no per-website extractors).

## Build and test

```bash
# Configure + build (presets: debug, release, build, coverage, gui, all)
cmake --preset debug
cmake --build --preset debug

# Run all tests
ctest --test-dir output/cmake-build-debug --output-on-failure

# Run a single test executable (one CTest test = one executable of subtests)
ctest --test-dir output/cmake-build-debug -R test_download_state --output-on-failure
# or run it directly:
./output/cmake-build-debug/tests/test_download_state
```

`scripts/run_tests.sh` does configure + build + ctest against `output/build`. All build trees live under `output/` (git-ignored). CI runs with AddressSanitizer/UBSan. Coverage: use the `coverage` preset (`AVAR_ENABLE_COVERAGE=ON`, switches to object libraries so gcov counters aren't duplicated), then `scripts/coverage_report.py`.

Tests are declared in `tests/CMakeLists.txt` via `avar_add_test(name TIMEOUT tier)` — a timeout tier (fast/unit/concurrent/daemon/integration/lifecycle) is mandatory. Integration tests spin up `scripts/http_test_server.py` locally.

GUI: `cd gui && npm run dev` (SPA on :56000, needs a daemon started with `avar daemon start --http --port=8000`), `npm run build`, `npm run dev:desktop` / `build:desktop` for Electron. `-DAVAR_BUILD_GUI=ON` produces `avar-gui` with the web UI embedded; `-DAVAR_BUILD_ALL=ON` embeds GUI + Electron into `avar`.

## Architecture

Client–server: the **daemon owns all download state**; clients (CLI, GUI, extensions) send it commands. CLI and daemon are the same executable. Request flow: client → `src/daemon/daemon_session.c` (routes locally in-process or over a transport per `daemon.session.mode` in config.json) → `daemon_transport.c` (HTTP / named pipe / Unix socket) → `daemon_rpc.c` (JSON-RPC dispatch) → `queue.c` / `download.c` / `config.c`. Live GUI updates go out via SSE/WebSocket. Never run the transfer engine in the CLI when `daemon.session.mode` is `remote`.

Two static libraries: `avar_core` (download engine, daemon, HTTP, config) and `avar_cli` (argtable3-based command layer). `*_testing` variants are compiled with `AVAR_TESTING=1`. Vendored deps (mongoose for HTTP/TLS server+client, mbedtls, cJSON, argtable3) live in `src/third_party/` as git submodules — new C deps go there as submodules, never into `src/` proper.

Download engine (`download.c` + `download_state/sync/io/segment/probe/config.c`): each download is a `DownloadJob`; segmented downloads split the file into parallel HTTP range requests via the thread pool. HLS (`.m3u8`, including AES-128) is handled by `stream_hls.c`. The download list persists in the `dm.items` array of `config.json`; per-download resume state (segment offsets) lives in a `state.json` next to the partial file.

Public headers go in `src/include/`, implementations in `src/`. C23 features are preferred; preserve API compatibility.

For the design rationale behind the download engine — the concurrency model, the performance tuning against IDM, and the durability/memory invariants — read [`DESIGN.md`](DESIGN.md). The most important points for making changes safely are below.

## Download engine performance and concurrency (deliberate design — do not regress)

The thread pool runs one download per worker; within a download, segments are parallel async connections in a single mongoose event loop. Mongoose moves at most one `recv()` per connection per poll iteration, so **anything blocking on the receive path directly caps throughput and makes the transfer rate jitter**. This shaped several choices that look like they could be "simplified" but must not be:

- **No blocking I/O on the receive path.** Disk writes go through the async writer (see below); `state.json` saves are throttled to `DL_PROGRESS_PERSIST_INTERVAL_MS` (500 ms) / `DL_PROGRESS_PERSIST_MIN_BYTES` (8 MiB); the `dm.items` rewrite of the whole `config.json` is throttled to `DL_UI_REFRESH_INTERVAL_MS` (1 s) during steady transfer via `dm_item_upsert_throttled()`. Rewriting config on every ~64 KiB was the single biggest stall vs IDM. Status transitions still publish immediately.
- **Segment concurrency is capped at `DL_MAX_SEGMENT_CONCURRENCY` (32)** and slot bookkeeping arrays are sized for it — `download_config_load()` clamps a larger configured value (exceeding it corrupted the stack). No byte range is ever written by two operations; the `MG_EV_HTTP_MSG` path clamps a server that returns more bytes than requested so it can't overwrite a neighbouring segment.
- **Keep-alive drain cap `DL_DRAIN_MAX_BYTES` (256 KiB):** a segment that reaches its neighbour's range only drains a short tail to reuse the connection; beyond that it reconnects. Without the cap a slot would receive-and-discard the rest of the file over the wire.
- **`MG_IO_SIZE` is 256 KiB** (CMake), not mongoose's 16 KiB default.
- **Transfer rate is a time-weighted EMA** (`avar_speed_ema`, τ = `DL_SPEED_EMA_TAU_MS` = 1.5 s), feeding both the CLI bar and the GUI's `bytesPerSecond`. Do not revert to a raw instantaneous delta — that was the cause of the GUI rate fluctuation.
- **Chunked bodies** (`Transfer-Encoding: chunked`) on the stream path are de-framed by an incremental RFC 9112 decoder before writing; a truncated chunked body must fail, not "complete". Writing raw framing to the file corrupts it silently.

Tunables live in `src/include/avar.h`; see the table in `DESIGN.md`.

## Crash durability and memory constraints (deliberate design — do not "optimize" away)

The app targets weak, crash-prone hardware (NAS boxes, power loss), so persistence ordering is load-bearing:

- **Content before state.** Downloaded data goes through the async writer (`src/file_async.c`, `AvarAsyncFile`): the transfer thread copies bytes into a swap buffer and a thread-pool worker does the `fwrite`+`fflush`, so disk latency never throttles the network loop. Before `state.json` is saved, `drain_temp_file()` blocks until the writer has flushed every queued byte to the OS, so a crash never leaves state claiming bytes that aren't on disk. A full disk sync (`fsync`) is deferred to file close because it would stall even the writer — keep that split. `finalize_download()` treats a failed `close_file()` as a failed download (never promotes an unflushed temp file to the destination).
  - The async writer is single-producer: callers serialize writes with `job->mutex`. Memory is bounded to two fixed buffers per open file (backpressure blocks the producer when both are in flight) — do not make the buffers grow or add unbounded queueing, that breaks the NAS memory budget below.
- **Items persist on add.** `dm_item_upsert()` writes the item to `config.json` as soon as a download is added or changes status, so it survives a crash; `dm_item_upsert_throttled()` exists only for steady-transfer progress updates. Don't batch or defer the upsert on add/status transitions.
- **RAM frugality is a requirement**, not a nicety — the daemon must run on low-memory NAS devices. Avoid buffering whole files or large per-job allocations; prefer streaming and fixed-size buffers.
