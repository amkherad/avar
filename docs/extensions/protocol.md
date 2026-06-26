---
title: Protocol
sort: 3
parent: Browser Extensions
---

# Extension Bridge Protocol

The browser extension communicates with Avar through an HTTP JSON bridge hosted by **Avar Desktop (Electron)**. The extension never talks to the daemon directly.

Default bridge URL: `http://127.0.0.1:18766`

## Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/v1/ping` | GET | Health check |
| `/v1` | POST | Envelope messages (v1 protocol) |
| `/extension/ping` | GET | Legacy health check |
| `/extension/download` | POST | Legacy direct download queue |
| `/extension/batch/{id}` | GET | Fetch stashed batch payload (GUI) |
| `/extension/add-download/{id}` | GET | Fetch stashed single-add payload (GUI) |
| `/extension/settings` | POST | Enable/disable bridge (GUI internal) |

## Envelope format (v1)

Request:

```json
{
  "protocol": "avar.extension",
  "version": 1,
  "type": "download.add.open",
  "id": "1234567890",
  "payload": {
    "url": "https://example.com/file.mp4",
    "streamKind": "direct",
    "filename": "file.mp4",
    "referer": "https://example.com/page",
    "pageUrl": "https://example.com/page",
    "pageTitle": "Example Page",
    "defaultQueueId": null,
    "title": "Add download"
  }
}
```

Success response:

```json
{
  "protocol": "avar.extension",
  "version": 1,
  "type": "download.add.open",
  "id": "1234567890",
  "ok": true,
  "payload": {
    "addId": "abc123",
    "title": "Add download"
  }
}
```

Error response:

```json
{
  "protocol": "avar.extension",
  "version": 1,
  "type": "download.add.open",
  "id": "1234567890",
  "ok": false,
  "error": "Missing url"
}
```

## Message types

| Type | Direction | Description |
|------|-----------|-------------|
| `ping` | Extension → Bridge | Health check; payload may include `extensionVersion` |
| `status` | Extension → Bridge | Request bridge/daemon status |
| `download.add` | Extension → Bridge | Queue download immediately (legacy / intercept) |
| `download.add.open` | Extension → Bridge | Stash item and open **Add download** dialog in GUI |
| `download.batch.open` | Extension → Bridge | Stash items and open **Add downloads** batch dialog |
| `url.probe` | Extension → Bridge | HEAD/probe URL for `size` and `filename` |
| `hls.list` | Extension → Bridge | List HLS master playlist variants |
| `queue.list` | Extension → Bridge | List daemon queues for settings picker |
| `queue.start` | Extension → Bridge | Start a queue by id |
| `queue.stop` | Extension → Bridge | Stop a queue by id |

### `download.add` payload

```json
{
  "url": "https://example.com/file.mp4",
  "streamKind": "direct",
  "referer": "https://example.com/",
  "queue": "queue-id",
  "startNow": true
}
```

### `download.batch.open` payload

```json
{
  "items": [
    {
      "url": "https://example.com/a.mp4",
      "streamKind": "direct",
      "filename": "a.mp4",
      "linkName": "a.mp4",
      "fileType": "video",
      "fileSize": 1048576,
      "originalUrl": "https://example.com/page",
      "referer": "https://example.com/page"
    }
  ],
  "pageUrl": "https://example.com/page",
  "pageTitle": "Example",
  "defaultQueueId": null,
  "title": "Add downloads"
}
```

### `url.probe` payload / response

Request: `{ "url": "…", "referer": "…" }`

Response: `{ "size": 1048576, "filename": "file.mp4" }` (either field may be null)

### `hls.list` payload / response

Request: `{ "url": "https://…/master.m3u8", "referer": "…" }`

Response: `{ "variants": [{ "url": "…", "label": "1080p", "resolution": "1920x1080" }] }`

## Flow

```
Browser extension (background.js)
       │  HTTP POST /v1  (2 s timeout)
       ▼
Extension bridge (Electron, port 18766)
       │  Stash payload → open GUI dialog
       │  JSON-RPC (daemon HTTP API)
       ▼
Avar daemon
       │
       ▼
Download engine
```

Before opening dialogs, the extension calls `ensureBridgeReachable`: ping the bridge, and if needed open `avar://focus` to wake Avar Desktop, then retry.

## Bridge ports

| Port | Service |
|------|---------|
| `18766` | Extension bridge (Electron) — **only port the extension uses** |
| `18765` | Daemon API proxy (Electron internal) |
| `8000` | Daemon HTTP API (default) |

The extension normalizes any stored bridge URL to localhost port **18766**. Other ports fall back to the default.

## Internal extension messages

These are `chrome.runtime.sendMessage` types between popup/content and background (not HTTP):

| Type | Purpose |
|------|---------|
| `avar-get-page-media` | Content script returns merged media + selection |
| `avar-list-media` | Background scans active tab |
| `avar-selection-changed` | Update context menu state |
| `avar-ping-bridge` | Bridge health for popup/widget |
| `avar-set-config` | Persist storage settings |
| `avar-open-add-download` | Open single review dialog |
| `avar-open-batch-add` | Open batch review dialog |
| `avar-open-downloads` | Selection widget bulk open |
| `avar-probe-size` | Proxy to bridge `url.probe` |
| `avar-expand-hls-items` | Proxy to bridge `hls.list` + expand |
| `avar-list-queues` | Proxy to bridge `queue.list` |

## Shared code

Protocol client: `extensions/shared/protocol.js`

Bridge server: `gui/electron/extension-bridge.cjs`

When modifying protocol behavior, update both the bridge handler and `protocol.js`, then document changes here and in `.agents/skills/extension-development/BEHAVIOR.md`.

## Legacy protocol

Older extension versions used `/extension/ping` and `/extension/download`. The v1 envelope protocol is preferred; legacy endpoints remain for backward compatibility.
