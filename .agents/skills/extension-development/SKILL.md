---
name: extension-development
description: Browser extension media capture for Avar in extensions/. Apply when editing extension scripts, manifests, or shared capture helpers. Enforces site-agnostic detection only.
---

# Browser Extension Development

Apply to all files under `extensions/`. For GUI bridge code, use **react-development** and **gui-code-style** instead.

## Core rule: no site-specific code

**Never** add logic keyed to a particular host, brand, or page layout.

Do **not**:

- Match hostnames (`youtube.com`, …)
- Match site-specific CSS classes (`VideoPage__video`, …)
- Map site-specific query params (`type=5` → 1080p, `ct=6` → mkv, …)
- Add per-site extractor modules or `if (location.hostname …)` branches
- Name files or symbols after a site (`youtube-hook.js`, `classifyYoutubeCdnUrl`, …)

If a site is not detected well enough, improve **generic** heuristics so the same code works everywhere.

## Layout

| Path | Role |
|------|------|
| `extensions/shared/` | Source of truth — edit here first |
| `extensions/chromium/` | Manifest V3 build (synced from `shared/`) |
| `extensions/firefox/` | Manifest V2 build (synced from `shared/`) |

Synced files are listed in `gui/vite-extensions.ts` (`syncSharedAssets`). After editing `shared/`, sync copies to both browser folders (build or manual copy).

## Approved capture techniques

Use only these layers. Each layer must stay site-agnostic.

### 1. Network observation (`capture.js` + background)

- `webRequest.onCompleted` with `responseHeaders`
- Classify via `AvarMedia.classifyCapturedRequest(url, headers)`
- Signals: `Content-Type` (`video/*`, `audio/*`, `video/x-matroska`), `Content-Disposition`, `Content-Length`, URL shape

### 2. DOM scan (`media.js` → `collectMediaItems`)

- `<video>`, `<audio>`, `<source>`, preload links, anchors with media extensions
- Inline script / HTML text regex for stream URLs
- `performance.getEntriesByType("resource")`

### 3. API response scan (`page-response-hook.js`, page world)

- Wrap `fetch` / `XHR` and scan **text** responses (JSON, HTML, JS) for embedded URLs
- Gate on generic `MEDIA_HINT_RE` (extensions, `mime=video`, `sig`, `expires`, HLS/DASH paths)
- Post to content script via `postMessage`

### 4. Dynamic player hooks (`media-hook.js`, page world)

- `MutationObserver` on `src` / new `video`/`audio` nodes
- Hook `HTMLMediaElement` / `HTMLSourceElement` `src` setter and `setAttribute("src", …)`
- Report URLs matching generic `MEDIA_URL_HINT_RE` (media extensions, signed `sig=`, stream path patterns)
- Post via `avar-media-urls` message

### 5. URL classification (`media.js`)

Generic signals only:

| Signal | Example |
|--------|---------|
| File extension | `.mp4`, `.m3u8`, `.mkv` |
| Stream path | `/hls/`, `videoplayback`, `hls_playlist` |
| Signed CDN query | `sig` + `expires` (any host) |
| MIME in query | `mime=video` |
| Response header | `video/mp4`, `video/x-matroska`, large `application/octet-stream` |

Use `looksLikeSignedMediaUrl()` and `SIGNED_MEDIA_URL_IN_TEXT_RE` for tokenized CDN links — **without** checking the hostname.

## Content script wiring (`content.js`)

1. Inject `page-response-hook.js` and `media-hook.js` into the page world
2. Listen for `avar-response-body` and `avar-media-urls`
3. Re-classify with `AvarMedia.classifyMediaUrl()` in the extension world
4. Merge with DOM + background captures via `AvarMedia.mergeMediaItems()`

## Adding a new technique

Before implementing, confirm:

1. It does not mention any specific site or CDN brand
2. It would plausibly help on multiple unrelated platforms
3. Classification lives in `media.js`, not in page hooks (hooks only discover raw URLs)
4. `shared/` is updated and synced to `chromium/` and `firefox/`
5. `web_accessible_resources` lists any new injected page scripts

## Do not

- Duplicate fetch/XHR wrapping in multiple page scripts without reason
- Parse video quality from opaque site-specific enums
- Hard-code referer or cookie rules per site in the extension (pass page URL as referer generically)
- Add npm dependencies to extension scripts (classic scripts, no bundler)

## Testing

Manual: load unpacked extension, open a media-heavy page, confirm popup lists URLs after playback. Verify signed CDN URLs (no file extension) appear via generic `sig`/`expires` detection.
