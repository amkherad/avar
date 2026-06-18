---
title: Configuration Overview
sort: 4
parent: Getting Started
---

# Configuration Overview

Avar stores its settings in a JSON file called `config.json`. The GUI keeps its own preferences separately in browser storage.

## Config file location

| Platform | Path |
|----------|------|
| **Windows** | `%APPDATA%\avar\config.json` |
| **Linux / macOS** | `$XDG_CONFIG_HOME/avar/config.json` or `~/.config/avar/config.json` |

Set `AVAR_INSTANCE=<name>` to use a separate config directory (`avar-<name>`) — useful for running multiple instances.

## Main sections

| Section | Purpose |
|---------|---------|
| `dm` | Download manager settings — items, queues, paths, segmentation, proxy |
| `daemon` | Session mode, server channels (HTTP, pipe, Unix socket), CORS, auth |
| `log` | Optional file logging |

A minimal config is created automatically on first run with sensible defaults.

## Changing settings

### CLI

```bash
avar config get dm.downloadPath
avar config set dm.downloadPath /home/user/Downloads
avar config reset dm.downloadPath
avar config save ~/backup-config.json
avar config load ~/backup-config.json
```

Keys use **dot notation** (e.g. `daemon.session.mode`, `dm.proxy.enabled`).

### Environment variables

Any config key can be overridden with an environment variable:

```
avar.<config-key>
```

Examples:

```bash
export avar.daemon.session.mode=remote
export avar.daemon.session.remotePort=9000
```

## Session mode

`daemon.session.mode` controls how the CLI talks to the engine:

| Mode | Behavior |
|------|----------|
| `local` | CLI runs download logic in-process (default) |
| `remote` | CLI delegates all work to the daemon via transport |

When `remote`, the CLI does not run the transfer engine itself. Set `daemon.session.transport` to `pipe`, `unix`, or `http`.

## Default paths

When not configured:

- **Temp directory** — platform data directory + `download-temp`
- **Download directory** — `~/Downloads` (or `$XDG_DOWNLOAD_DIR` on Linux)

## GUI preferences

Theme, language, session URLs, and keyboard shortcuts are stored in `localStorage` under `avar.gui.config`. They are independent of `config.json` and travel with the browser or Electron profile.

See [Configuration file reference]({{ site.baseurl }}/architecture/config-file.html) for the full schema.
