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

## Crash durability and memory constraints (deliberate design — do not "optimize" away)

The app targets weak, crash-prone hardware (NAS boxes, power loss), so persistence ordering is load-bearing:

- **Content before state.** Downloaded data is `fflush()`ed to the OS on every write; `state.json` is only written after the data it describes is flushed, so a crash never leaves state claiming bytes that aren't on disk. A full disk sync (`fsync`) is deferred to file close because it would stall the transfer loop — keep that split.
- **Items persist on add.** `dm_item_upsert()` writes the item to `config.json` as soon as a download is added or changes status, so it survives a crash; `dm_item_upsert_throttled()` exists only for steady-transfer progress updates. Don't batch or defer the upsert on add/status transitions.
- **RAM frugality is a requirement**, not a nicety — the daemon must run on low-memory NAS devices. Avoid buffering whole files or large per-job allocations; prefer streaming and fixed-size buffers.
