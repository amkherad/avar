---
title: Overview
sort: 1
parent: Architecture
---

# Architecture Overview

Avar follows a client–server model where the **daemon** owns all download state and the **clients** (CLI, GUI, extensions) send commands to it.

## Component diagram

```
┌─────────────────────────────────────────────────────────┐
│                        Clients                          │
│  ┌──────────┐   ┌──────────┐   ┌─────────────────────┐  │
│  │   CLI    │   │   GUI    │   │ Browser extension   │  │
│  │  (avar)  │   │ (React)  │   │ (Chromium/Firefox)  │  │
│  └────┬─────┘   └────┬─────┘   └──────────┬──────────┘  │
└───────┼──────────────┼────────────────────┼─────────────┘
        │              │                    │
        ▼              ▼                    ▼
   pipe / unix     HTTP JSON-RPC      Extension bridge
        │              │                    │
        └──────────────┼────────────────────┘
                       ▼
              ┌─────────────────┐
              │  Daemon server  │
              │   (mongoose)    │
              └────────┬────────┘
                       │
              ┌────────┴────────┐
              ▼                 ▼
        ┌──────────┐     ┌──────────┐
        │ JSON-RPC │     │ SSE / WS │
        │  handler │     │  events  │
        └────┬─────┘     └──────────┘
             │
     ┌───────┼───────┐
     ▼       ▼       ▼
  queue.c download.c config.c
     │       │       │
     └───────┴───────┘
             │
             ▼
      config.json + state.json
```

## Request flow

1. A client sends a command (CLI argv, GUI button, or extension message)
2. The **daemon session** layer routes it — locally in-process or over a transport
3. The **daemon RPC** handler dispatches JSON-RPC methods
4. **queue.c** or **download.c** executes the operation
5. State is persisted to `config.json` and per-download `state.json`
6. Live updates are pushed to connected GUI clients via SSE or WebSocket

## Session modes

| Mode | When to use |
|------|-------------|
| **local** | Single-machine use; CLI runs logic in-process |
| **remote** | CLI controls a background or remote daemon; no engine in the CLI process |

See [Daemon mode]({{ site.baseurl }}/architecture/daemon-mode.html) for transport details.

## Build targets

| Target | Description |
|--------|-------------|
| `avar` | CLI + daemon (default build) |
| `avar-gui` | Same backend with embedded web UI served over HTTP |

The GUI can also run standalone — built with `npm run build` and served as static files, or packaged with Electron via `npm run build:desktop`.

## Multi-instance support

Set the `AVAR_INSTANCE` environment variable to run multiple independent Avar instances. Each instance gets its own config directory, pipe name, and port offset.

```bash
AVAR_INSTANCE=work avar daemon start --http --port=8001
```
