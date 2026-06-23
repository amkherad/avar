---
title: daemon
sort: 5
parent: CLI Reference
---

# daemon

Control the Avar background daemon — start, stop, monitor, and install as a system service.

```
avar daemon <subcommand> [options]
```

The daemon is the same `avar` binary. It runs the download engine, persists state, and exposes a JSON-RPC API.

## Subcommands

### start

```
avar daemon start
    [--attached]
    [--http] [--pipe] [--unix] [--container]
    [--port=<n>]
    [--pipeName=<name>]
    [--unixPath=<path>]
    [--pidFile=<path>]
    [--no-detach]
```

Start the daemon. By default it **detaches** into the background unless `--attached` or `--no-detach` is passed.

| Option | Description |
|--------|-------------|
| `--attached` | Run in the foreground (do not detach) |
| `--http` | Enable HTTP channel (on by default) |
| `--pipe` | Enable named pipe channel (on by default) |
| `--unix` | Enable Unix domain socket channel |
| `--container` | Container-friendly mode |
| `--port` | HTTP listen port (default: **8000**) |
| `--pipeName` | Named pipe name (Windows) |
| `--unixPath` | Unix socket path |
| `--pidFile` | Write PID to this file |
| `--no-detach` | Same as `--attached` |

**Examples:**

```bash
avar daemon start --http --port=8000
avar daemon start --attached          # foreground, for debugging
avar daemon start --unix --unixPath=/tmp/avar.sock
```

### stop

```
avar daemon stop [--wait] [--kill]
```

Stop the running daemon.

| Option | Description |
|--------|-------------|
| `--wait` | Wait until the daemon has exited |
| `--kill` | Force kill if graceful shutdown fails |

### restart

```
avar daemon restart
```

Stop and start the daemon.

### reload

```
avar daemon reload
```

Reload configuration without a full restart. On Windows this uses an RPC call; on Unix it sends `SIGHUP`.

### attach

```
avar daemon attach
```

Open a live terminal UI showing queue status and active download progress. Press `q` to quit the UI (the daemon keeps running).

### logs

```
avar daemon logs [--follow]
```

Print daemon log output. Use `--follow` to stream new entries (like `tail -f`).

### status

```
avar daemon status
```

Show daemon status — running state, PID, uptime, and channel information.

### ping

```
avar daemon ping
```

Check reachability. Prints `pong` when the daemon responds.

### install

```
avar daemon install
    [--systemd] [--launchd] [--windows-service]
    [--user]
```

Install the daemon as an OS service. **One platform flag is required.**

| Flag | Platform |
|------|----------|
| `--systemd` | Linux (systemd unit) |
| `--launchd` | macOS (launchd plist) |
| `--windows-service` | Windows service |
| `--user` | User-level service (systemd / launchd) |

```bash
avar daemon install --systemd --user
avar daemon install --launchd --user
avar daemon install --windows-service
```

### uninstall

```
avar daemon uninstall
    [--systemd] [--launchd] [--windows-service]
    [--user]
```

Remove the installed service. Use the same platform flags as `install`.

## Default channels

| Channel | Default | Details |
|---------|---------|---------|
| HTTP | Enabled | `0.0.0.0:8000`, CORS enabled |
| Named pipe | Enabled | `\\.\pipe\avar-<user>` (Windows) or `/tmp/avar.pipe` (Unix) |
| Unix socket | Disabled | `/tmp/avar.sock` when enabled |
| HTTPS | Disabled | Port 8001, requires cert/key files |

The CLI uses the **pipe** transport by default to talk to the daemon. The GUI uses **HTTP**.

## HTTP API

When HTTP is enabled, the daemon exposes:

| Endpoint | Purpose |
|----------|---------|
| `/api/health` | Health JSON |
| `/api/stats` | System statistics (poll mode only; push clients receive stats on SSE/WebSocket) |
| `/api/rpc` | JSON-RPC 2.0 |
| `/api/events` | Server-sent events (live updates) |
| `/api/ws` | WebSocket (live updates) |

See [Daemon mode]({{ site.baseurl }}/architecture/daemon-mode.html) for architecture details.
