# Avar Browser Extensions

Browser extensions that detect media on web pages and open Avar download dialogs via a local HTTP bridge.

## Documentation

| Audience | Location |
|----------|----------|
| Users | `docs/extensions/` — start at [behavior.md](../docs/extensions/behavior.md) |
| Agents | `.agents/skills/extension-development/BEHAVIOR.md` — behavior contract |
| Developers | [architecture.md](../docs/extensions/architecture.md), [protocol.md](../docs/extensions/protocol.md) |

## Layout

| Path | Browsers |
|------|----------|
| `shared/` | **Source of truth** — edit here first |
| `chromium/` | Chrome, Edge, Opera (Manifest V3) |
| `firefox/` | Firefox (Manifest V2) |
| `packages/` | Built ZIPs (`avar-chromium.zip`, `avar-firefox.zip`) |

Synced from `shared/` (see `gui/vite-extensions.ts`): `media.js`, `protocol.js`, `capture.js`, `hls.js`, `context-menu.js`, `download-intercept.js`, `popup.js`, `popup.html`, `page-response-hook.js`, `media-hook.js`, `content.js`.

Browser-specific: `background.js`, `manifest.json`, icons.

## Install (development)

1. Start the daemon: `avar daemon start --http --port=8000`
2. Open **Avar Desktop** (Electron) — required for the extension bridge
3. Enable **Listen for browser extension connections** in GUI settings
4. Load unpacked: `chromium/` or `firefox/`
5. After editing `shared/`, run `npm run dev` or build in `gui/` to sync copies

## Bridge

- URL: `http://127.0.0.1:18766` (Electron only)
- Client: `shared/protocol.js`
- Server: `gui/electron/extension-bridge.cjs`
- Protocol: v1 envelope on `POST /v1`, health on `GET /v1/ping`

The extension opens **Add download** / **Add downloads** review dialogs — it does not queue silently (except optional browser-download intercept).

## Core modules

| File | Role |
|------|------|
| `media.js` | URL classification, DOM scan, merge/sort/filter |
| `content.js` | Page hooks, selection widget, media merge |
| `capture.js` | `webRequest` network capture |
| `page-response-hook.js` | fetch/XHR response body scan (page world) |
| `media-hook.js` | Dynamic `video`/`audio` src hooks (page world) |
| `popup.js` | Toolbar popup UI |
| `hls.js` | HLS master playlist expansion |
| `download-intercept.js` | Native browser download grab |
| `context-menu.js` | Right-click menus |
| `background.js` | Bridge client, orchestration (per browser) |

## Site-agnostic rule

Never add hostname checks or per-site extractors. Improve generic heuristics in `media.js`. See `.agents/skills/extension-development/SKILL.md`.

## Messaging protocol (v1)

See [docs/extensions/protocol.md](../docs/extensions/protocol.md) for full message types (`download.add.open`, `download.batch.open`, `url.probe`, `hls.list`, `queue.list`, …).
