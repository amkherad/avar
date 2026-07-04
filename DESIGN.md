# Avar — High-Level Design

This document captures the *why* behind Avar's download engine: the architecture,
the performance and durability constraints it is built around, and the invariants
that later changes must not break. It complements the user-facing docs in
[`docs/architecture/`](docs/architecture/) (which describe *what* the system does)
and [`CLAUDE.md`](CLAUDE.md) (the working rules). Much of this rationale comes from
a focused performance/durability effort; the numbers below are the outcome of
measuring against Internet Download Manager (IDM) on real transfers.

## 1. System model

Avar is a client–server download manager. A single **daemon** owns all download
state; **clients** (CLI, React/Electron GUI, browser extensions) send it commands
and never touch the transfer engine directly.

```
CLI / GUI / extension
        │  argv · HTTP JSON-RPC · extension bridge
        ▼
daemon_session  →  daemon_transport (HTTP / pipe / unix socket)  →  daemon_rpc
        ▼
   queue.c · download.c · config.c
        ▼
   config.json  +  per-download state.json  +  temp/partial files
```

The CLI and daemon are one executable. When `daemon.session.mode` is `remote`,
clients must delegate over the transport and must **not** run the transfer engine
locally.

## 2. Download engine

### Concurrency model

- The global **thread pool** (`thread_pool.c`) runs **one download per worker**.
- Within a download, **segments are parallel async connections** driven by a single
  mongoose event loop (`mg_mgr_poll`). Each segment is a `ChunkSlot` with its own
  connection and byte range.

This is the same shape IDM uses. The bottleneck versus IDM was never missing
parallelism — it was the event loop **stalling on disk and config I/O between
socket reads**, plus one scheduling bug that wasted bandwidth. Everything below
follows from "never make the poll loop wait."

### Segmentation and overlap safety

Ranges are handed out by the reservation logic (`segment_next_reserve_end`,
in-flight predicates, done-range checks in `download_segment.c` /
`download_state.c`). The safety property is: **no byte range is ever written by
two operations**, so nothing is downloaded-and-written twice. Hardening in place:

- The `MG_EV_HTTP_MSG` write path is clamped so a server returning **more** bytes
  than requested cannot overwrite a neighbouring segment.
- In-flight range collection is bounded and segment concurrency is capped at
  `DL_MAX_SEGMENT_CONCURRENCY` (32). The slot bookkeeping arrays are sized for this
  cap; a configured concurrency above it previously corrupted the stack, so
  `download_config_load()` clamps it.

### Keep-alive drain policy

When a segment connection reaches its neighbour's range and cannot extend, the
open-ended response still has bytes coming. Consuming them keeps the connection
alive for reuse — but only up to a point. Draining more than
`DL_DRAIN_MAX_BYTES` (one chunk, 256 KiB) is slower than just closing and
reconnecting, so beyond that the slot reconnects. Before this cap existed, a slot
could receive and **discard** the entire remainder of the file (per slot, until a
120 s stall timeout) — data pulled over the wire twice or more.

### Transfer-Encoding: chunked

Stream-mode downloads may arrive `Transfer-Encoding: chunked`. The body is written
straight from the socket, so the framing must be stripped or it corrupts the file
(this was a real, silent bug: a 50 MB download came out exactly the framing-bytes
too large). `download.c` contains an incremental RFC 9112 chunked decoder with
completeness detection — a truncated chunked body now **fails** instead of falsely
"completing", and there is a fallback if a range reply unexpectedly arrives chunked.

## 3. Performance: keep the poll loop moving

Mongoose moves at most one `recv()` per connection per poll iteration, so any work
done inline on the receive path directly caps throughput and makes the rate jitter.
The receive path therefore does **no** blocking I/O:

- **Disk writes are asynchronous** (`file_async.c`, `AvarAsyncFile`). The transfer
  thread copies received bytes into one of two fixed swap buffers and returns; a
  thread-pool worker does the `fwrite`+`fflush`. Memory is bounded to two buffers
  per open file — when both are in flight the producer blocks (backpressure),
  which is what keeps footprint bounded on weak hardware.
- **`fsync`/`_commit` happens only at file close** (finalize / pause / stop), never
  per write.
- **`state.json` is persisted at most every `DL_PROGRESS_PERSIST_INTERVAL_MS`
  (500 ms) or per `DL_PROGRESS_PERSIST_MIN_BYTES` (8 MiB)**, not per receive.
- **The `dm.items` entry in `config.json` is rewritten at most every
  `DL_UI_REFRESH_INTERVAL_MS` (1 s) during steady transfer.** Rewriting the whole
  config file on every 64 KiB (~800×/s at 50 MB/s) was the single biggest stall.
  **Status transitions bypass the throttle and publish immediately** (see §4).
- **`MG_IO_SIZE` is 256 KiB** (up from mongoose's 16 KiB default), so each poll
  iteration moves up to 16× more data per connection and the TLS record path
  benefits too.

### Transfer-rate measurement

The GUI's rate used to fluctuate wildly. The cause was measurement, not the
network: a raw instantaneous delta over irregular ~200 ms windows, amplified by the
event-loop stalls making byte counts advance in bursts. It is now a time-weighted
exponential moving average (`avar_speed_ema`, τ = `DL_SPEED_EMA_TAU_MS` = 1.5 s)
feeding both the CLI progress bar and the `bytesPerSecond` field the GUI reads.
Removing the stalls made the underlying counts advance smoothly as well.

## 4. Durability: crash- and power-loss-safe

Target hardware includes NAS boxes that can lose power at any moment, so ordering
is load-bearing. The invariant is **content before state**: a crash may lose recent
bytes, but persisted state must never claim bytes that are not on disk.

- **Content before state.** Before `state.json` records a byte range, the async
  writer is drained (`drain_temp_file()`) so those bytes have reached the OS. A
  failed `close_file()` fails the download — an unflushed temp file is never
  promoted to the destination.
- **Items persist on add.** `dm_item_upsert()` writes the item to `config.json` as
  soon as a download is added or changes status, so it survives a crash. Progress
  ticks use `dm_item_upsert_throttled()`; add/status transitions must not be
  throttled or deferred.
- **Atomic file replacement.** `config.json`, `state.json`, and bookmarks are
  written to a temp file, `fsync`ed via `file_sync_to_disk()`, then `rename()`d, so
  a power loss cannot leave a half-written file in place of a good one.
- **`fsync` is deferred to close** on the *download data* path (§3) because syncing
  on the hot loop would stall it — but the throttled state/config writers do sync,
  since they are already off the per-receive path.

## 5. Memory

RAM frugality is a requirement, not a nicety — the daemon must run on low-memory
NAS devices. No path buffers a whole file or makes large per-job allocations; the
async writer's two-buffer bound and the segment-concurrency cap exist specifically
to keep the footprint predictable under load.

## 6. Key tunables

All in `src/include/avar.h` unless noted. Changing these trades throughput against
crash-window and memory; the current values were tuned against IDM.

| Constant | Value | Purpose |
|----------|-------|---------|
| `MG_IO_SIZE` (CMake) | 256 KiB | Bytes moved per connection per poll iteration |
| `DL_MAX_SEGMENT_CONCURRENCY` | 32 | Hard cap on parallel segment connections; sizes slot arrays |
| `DL_DRAIN_MAX_BYTES` | 256 KiB | Max tail drained to keep a connection alive before reconnecting |
| `DL_PROGRESS_PERSIST_INTERVAL_MS` | 500 ms | Min time between `state.json` saves |
| `DL_PROGRESS_PERSIST_MIN_BYTES` | 8 MiB | Min bytes between `state.json` saves |
| `DL_UI_REFRESH_INTERVAL_MS` | 1 s | Min time between `dm.items` rewrites during transfer |
| `DL_SPEED_EMA_TAU_MS` | 1.5 s | Time constant for the transfer-rate EMA |
| `ASYNC_FILE_BUFFER_SIZE` (`file_async.c`) | 256 KiB | Each of the two async-writer swap buffers |

## 7. Non-obvious invariants (do not regress)

1. Nothing blocking on the mongoose receive path — route disk writes through
   `AvarAsyncFile`, keep state/config persistence throttled.
2. Content is durable before state references it; status/add upserts are immediate
   while progress upserts are throttled.
3. No byte range is written by two operations; keep the concurrency cap and the
   over-read clamp.
4. The chunked decoder must stay on the stream path — writing raw framing corrupts
   files silently.
5. The async writer is single-producer (callers serialize with `job->mutex`) and
   memory-bounded to two buffers per file — do not add unbounded queueing or grow
   the buffers.
