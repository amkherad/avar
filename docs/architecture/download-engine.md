---
title: Download Engine
sort: 3
parent: Architecture
---

# Download Engine

The download engine handles HTTP/HTTPS transfers, segmented parallel downloads, resume, HLS streams, and proxy routing.

## Download lifecycle

```
queued → downloading → completed
              ↓    ↑
           paused  resumed
              ↓
           stopped / failed
```

Each download is stored in the `dm.items` array in `config.json`. Per-download resume state (segment positions, partial file offsets) is stored in a separate `state.json` file alongside the partial download.

## Segmented downloads

When segmentation is enabled, Avar splits a file into parallel range requests for higher throughput:

```json
{
  "dm": {
    "segmentation": {
      "enabled": true,
      "strategy": "balanced",
      "concurrency": 4,
      "chunkSize": "8MiB",
      "minFileSize": "16MiB"
    }
  }
}
```

| Setting | Description |
|---------|-------------|
| `enabled` | Turn segmentation on or off |
| `strategy` | `balanced` or `left-heavy` segment distribution |
| `concurrency` | Number of parallel segments |
| `chunkSize` | Size of each segment |
| `minFileSize` | Minimum file size before segmentation applies |

Segmentation requires the server to support HTTP range requests.

## Resume

If a download is interrupted (paused, stopped, or crashed), Avar resumes from the last known position using range requests and the saved `state.json`. No data already written to disk is re-downloaded.

## HLS streams

Avar supports HTTP Live Streaming (`.m3u8` playlists) including AES-128 encrypted segments. HLS downloads are handled by the stream module and produce a single output file.

## Proxy support

### Global proxy

Configure a proxy for all downloads that do not specify their own:

```json
{
  "dm": {
    "proxy": {
      "enabled": true,
      "type": "socks5",
      "host": "127.0.0.1",
      "port": "1080",
      "username": "",
      "password": "",
      "noProxy": "localhost,127.0.0.1"
    }
  }
}
```

Supported types: `http`, `https`, `socks5`.

### Per-download proxy

Override the global proxy for a single download via CLI:

```bash
avar https://example.com/file.zip --proxy=socks5://127.0.0.1:1080
```

### Environment variables

Standard proxy environment variables (`HTTP_PROXY`, `HTTPS_PROXY`, `ALL_PROXY`, `NO_PROXY`) are respected when no config or per-download override is set.

## Progress display

Progress units are configured under `dm.progress`:

```json
{
  "dm": {
    "progress": {
      "sizeUnit": "MiB",
      "speedUnit": "MiB/s",
      "style": "segmented"
    }
  }
}
```

## Download statuses

| Status | Description |
|--------|-------------|
| `queued` | Waiting for a slot in the queue |
| `downloading` | Actively transferring data |
| `paused` | Paused by user; resume state saved |
| `stopped` | Stopped; retained in the list |
| `completed` | Successfully finished |
| `failed` | Error occurred; retry count tracked |

## Supported protocols

Currently Avar supports **HTTP** and **HTTPS** URLs. BitTorrent and other protocols are planned but not yet implemented.
