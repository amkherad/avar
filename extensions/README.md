# Avar Browser Extensions

Browser extensions that send media links from web pages to the Avar download manager.

## Layout

| Path | Browsers |
|------|----------|
| `chromium/` | Chrome, Edge, Opera (Manifest V3) |
| `firefox/` | Firefox (Manifest V2) |
| `shared/` | Shared protocol and media discovery helpers |

## Install (development)

1. Start the Avar daemon with HTTP enabled (default `http://127.0.0.1:8000`).
2. Open the Avar GUI or Electron app.
3. Open **Settings** → **Browser integration** and install the extension for your browser.
4. Load the unpacked extension folder (`chromium/` or `firefox/`) from this directory.

## Configuration

The extension stores the Avar **bridge URL** in browser storage. When Avar Desktop (Electron) is running, the extension auto-detects the dedicated bridge at `http://127.0.0.1:18766`. For web-only development, it falls back to the Vite dev server at `http://127.0.0.1:56821`.

## Messaging protocol (v1)

Transport: HTTP JSON on the bridge port.

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/v1/ping` | GET | Health check |
| `/v1` | POST | Envelope messages (`ping`, `status`, `download.add`) |
| `/extension/ping` | GET | Legacy health check |
| `/extension/download` | POST | Legacy download queue |

Envelope format:

```json
{
  "protocol": "avar.extension",
  "version": 1,
  "type": "download.add",
  "id": "1234567890",
  "payload": { "url": "https://example.com/file.mp4" }
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

Downloads are forwarded to the active daemon session via JSON-RPC `download.add`.
