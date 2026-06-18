---
title: Configuration File
sort: 5
parent: Architecture
---

# Configuration File

Avar persists all daemon and download-manager state in `config.json`.

## File location

| Platform | Default path |
|----------|--------------|
| Windows | `%APPDATA%\avar\config.json` |
| Linux / macOS | `$XDG_CONFIG_HOME/avar/config.json` |

With `AVAR_INSTANCE=<name>`: `avar-<name>` subdirectory.

## Full schema

```json
{
  "dm": {
    "items": [],
    "queues": [],
    "tempPath": "",
    "downloadPath": "",
    "progress": {
      "sizeUnit": "MiB",
      "speedUnit": "MiB/s",
      "style": "segmented"
    },
    "segmentation": {
      "enabled": true,
      "strategy": "balanced",
      "concurrency": 4,
      "chunkSize": "8MiB",
      "minFileSize": "16MiB"
    },
    "proxy": {
      "enabled": false,
      "type": "http",
      "host": "",
      "port": "",
      "username": "",
      "password": "",
      "noProxy": ""
    }
  },
  "daemon": {
    "session": {
      "mode": "local",
      "transport": "pipe",
      "remoteHost": "127.0.0.1",
      "remotePort": 8000
    },
    "server": {
      "detach": true,
      "containerMode": false,
      "pidFile": "",
      "authToken": "",
      "autoShutdown": "never",
      "autoShutdownIdleSeconds": 60,
      "cors": {
        "enabled": true,
        "allowOrigin": "*"
      },
      "channels": {
        "http": {
          "enabled": true,
          "bind": "0.0.0.0",
          "port": 8000
        },
        "https": {
          "enabled": false,
          "bind": "0.0.0.0",
          "port": 8001,
          "certFile": "",
          "keyFile": ""
        },
        "pipe": {
          "enabled": true,
          "name": ""
        },
        "unix": {
          "enabled": false,
          "path": "/tmp/avar.sock"
        }
      }
    }
  },
  "log": {
    "file": {
      "enabled": false,
      "path": ""
    }
  }
}
```

## Section reference

### `dm` — Download manager

| Key | Type | Description |
|-----|------|-------------|
| `items` | array | All download entries |
| `queues` | array | Queue definitions |
| `tempPath` | string | Global temp directory |
| `downloadPath` | string | Global download directory |
| `progress.sizeUnit` | string | Display unit for sizes (`KiB`, `MiB`, `GiB`) |
| `progress.speedUnit` | string | Display unit for speed |
| `progress.style` | string | Progress bar style |
| `segmentation.*` | — | Parallel download settings |
| `proxy.*` | — | Global proxy settings |

### `daemon.session` — Client routing

| Key | Type | Description |
|-----|------|-------------|
| `mode` | string | `local` or `remote` |
| `transport` | string | `pipe`, `unix`, or `http` |
| `remoteHost` | string | Daemon host for remote mode |
| `remotePort` | number | Daemon HTTP port for remote mode |

### `daemon.server` — Daemon process

| Key | Type | Description |
|-----|------|-------------|
| `detach` | boolean | Detach on start |
| `containerMode` | boolean | Container-friendly behavior |
| `pidFile` | string | PID file path |
| `authToken` | string | Bearer token for HTTP API |
| `autoShutdown` | string | `never` or `idle` |
| `autoShutdownIdleSeconds` | number | Idle timeout before shutdown |
| `cors.enabled` | boolean | Enable CORS headers |
| `cors.allowOrigin` | string | Allowed origin(s) |
| `channels.*` | — | Listen channel configuration |

### `log` — Logging

| Key | Type | Description |
|-----|------|-------------|
| `file.enabled` | boolean | Write logs to a file |
| `file.path` | string | Log file path |

## Per-download state

Each active download may have a `state.json` file in the temp directory containing segment positions, byte offsets, and retry counts. This file is managed automatically and should not be edited manually.

## Environment overrides

Any key can be overridden at runtime:

```
avar.<dot.notation.key>=<value>
```

Example: `avar.daemon.server.channels.http.port=9000`

## GUI configuration

GUI preferences (theme, language, sessions, shortcuts) are stored separately in browser `localStorage` and are not part of `config.json`.

## Managing config from the CLI

```bash
avar config get dm.segmentation.concurrency
avar config set dm.segmentation.concurrency 8
avar config save ~/avar-backup.json
avar config load ~/avar-backup.json
avar config reset-all
```

See [config command]({{ site.baseurl }}/cli/config.html).
