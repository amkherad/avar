---
title: Protocol
sort: 3
parent: Browser Extensions
---

# Extension Bridge Protocol

The browser extension communicates with Avar through an HTTP JSON bridge. The GUI or Electron app hosts the bridge; the extension never talks to the daemon directly.

## Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/v1/ping` | GET | Health check |
| `/v1` | POST | Envelope messages |
| `/extension/ping` | GET | Legacy health check |
| `/extension/download` | POST | Legacy download queue |

## Envelope format (v1)

Request:

```json
{
  "protocol": "avar.extension",
  "version": 1,
  "type": "download.add",
  "id": "1234567890",
  "payload": {
    "url": "https://example.com/file.mp4"
  }
}
```

Response:

```json
{
  "protocol": "avar.extension",
  "version": 1,
  "type": "download.add",
  "id": "1234567890",
  "ok": true,
  "payload": {}
}
```

## Message types

| Type | Direction | Description |
|------|-----------|-------------|
| `ping` | Extension → Bridge | Health check |
| `status` | Extension → Bridge | Request bridge/daemon status |
| `download.add` | Extension → Bridge | Queue a new download URL |

## Flow

```
Browser extension
       │  HTTP POST /v1
       ▼
Extension bridge (GUI / Electron)
       │  JSON-RPC download.add
       ▼
Avar daemon
       │
       ▼
Download engine
```

The bridge translates extension messages into daemon JSON-RPC calls on the active session.

## Bridge ports

| Port | Service |
|------|---------|
| `18766` | Extension bridge (Electron) |
| `18765` | Daemon API proxy (Electron internal) |
| `8000` | Daemon HTTP API (default) |
| `56821` | Vite dev server (web dev fallback) |

## Shared code

Media detection logic lives in `extensions/shared/media.js` and is synced to both `chromium/` and `firefox/` builds. When modifying detection behavior, edit `shared/` first.

## Legacy protocol

Older extension versions used `/extension/ping` and `/extension/download`. The v1 envelope protocol is preferred; legacy endpoints remain for backward compatibility.
