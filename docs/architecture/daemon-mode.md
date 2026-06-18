---
title: Daemon Mode
sort: 2
parent: Architecture
---

# Daemon Mode

The Avar daemon is the central process that runs downloads, persists state, and serves the JSON-RPC API. The CLI and daemon share the same binary.

## Starting the daemon

```bash
avar daemon start --http --port=8000
```

By default the daemon detaches into the background. Use `--attached` to keep it in the foreground.

## Session routing

The `daemon.session` configuration controls how clients reach the engine:

```json
{
  "daemon": {
    "session": {
      "mode": "local",
      "transport": "pipe",
      "remoteHost": "127.0.0.1",
      "remotePort": 8000
    }
  }
}
```

### Local mode

In `local` mode, the CLI executes download logic in-process. The daemon can still run separately for the GUI, but the CLI does not require it.

### Remote mode

In `remote` mode, the CLI forwards commands to the daemon. The transfer engine does **not** run inside the CLI process. This is the recommended setup when the daemon runs as a background service.

### Transports

| Transport | Used by | Details |
|-----------|---------|---------|
| `pipe` | CLI (default) | Named pipe on Windows; FIFO on Unix |
| `unix` | CLI | Unix domain socket |
| `http` | GUI, remote CLI | HTTP JSON-RPC on configured port |

## Server channels

The daemon can listen on multiple channels simultaneously:

```json
{
  "daemon": {
    "server": {
      "channels": {
        "http": { "enabled": true, "bind": "0.0.0.0", "port": 8000 },
        "https": { "enabled": false, "bind": "0.0.0.0", "port": 8001 },
        "pipe": { "enabled": true, "name": "..." },
        "unix": { "enabled": false, "path": "/tmp/avar.sock" }
      }
    }
  }
}
```

## JSON-RPC API

The HTTP channel exposes JSON-RPC 2.0 at `/api/rpc`.

### Available methods

| Method | Description |
|--------|-------------|
| `ping` | Health check |
| `health` | Detailed health status |
| `system.stats` | CPU, memory, disk, network stats |
| `download.add` | Queue a new download |
| `downloads.list` | List all downloads |
| `queue.list` | List queues |
| `queue.add` | Create a queue |
| `queue.remove` | Remove a queue |
| `queue.edit` | Update queue settings |
| `queue.start` | Start a queue |
| `queue.stop` | Stop a queue |
| `cli.exec` | Execute a CLI command on the daemon |
| `logs.get` | Fetch log entries |
| `daemon.reload` | Reload configuration |

### Live updates

Connected clients can subscribe to real-time events:

| Endpoint | Protocol |
|----------|----------|
| `/api/events` | Server-sent events (SSE) |
| `/api/ws` | WebSocket |

The GUI uses these for live download progress without polling.

## Authentication

An optional `daemon.server.authToken` can be set. When configured, clients must send the token as a bearer header. The GUI stores tokens per session.

## CORS

Cross-origin access is enabled by default (`daemon.server.cors.enabled: true`, `allowOrigin: "*"`). Restrict this in production deployments where the GUI is hosted on a different origin.

## Service installation

The daemon can be registered as an OS service so it starts automatically:

```bash
avar daemon install --systemd --user    # Linux
avar daemon install --launchd --user    # macOS
avar daemon install --windows-service   # Windows
```

## Embedded GUI

When built with `-DAVAR_BUILD_GUI=ON`, the daemon serves the React SPA from its HTTP channel. No separate web server is needed — open `http://localhost:8000` after starting `avar-gui`.

## Auto shutdown

Configure `daemon.server.autoShutdown` to stop the daemon when idle:

| Value | Behavior |
|-------|----------|
| `never` | Keep running (default) |
| `idle` | Shut down after `autoShutdownIdleSeconds` with no active downloads |
