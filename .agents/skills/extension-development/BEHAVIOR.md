# Extension Behavior Contract

**Read this file before changing extension detection, UI, bridge integration, or settings.** User-facing docs: `docs/extensions/behavior.md`. Architecture: `docs/extensions/architecture.md`.

This document is the agent-maintained contract for **preserving existing behavior** when editing `extensions/`.

---

## Invariants (do not break)

1. **Site-agnostic detection only** — no hostname checks, site-specific CSS, or per-site modules. See SKILL.md core rule.
2. **Bridge target** — extensions talk only to `http://127.0.0.1:18766` (Electron). `normalizeBridgeUrl` rejects other localhost ports.
3. **Review-before-queue** — popup and context menu open `download.add.open` / `download.batch.open` dialogs; they do not silently queue (except download intercept path which uses `openSingleAdd`).
4. **Classification in extension world** — page hooks (`media-hook.js`, `page-response-hook.js`) only post raw URLs/text; `AvarMedia.classifyMediaUrl` runs in content/background.
5. **Edit `shared/` first** — sync to `chromium/` and `firefox/` via `gui/vite-extensions.ts` (`syncSharedAssets`).
6. **No npm/bundler** — classic scripts only; expose APIs on `globalThis`.

---

## Detection layers (all must remain active)

| Layer | File | Trigger | Output |
|-------|------|---------|--------|
| DOM scan | `media.js` `collectMediaItems` | Popup refresh, content collect | Classified items |
| DOM video | `media.js` `collectDomVideoItems` | Same | Accepts extensionless video `src` |
| Network | `capture.js` | `webRequest.onCompleted` + headers | Per-tab map |
| Response body | `page-response-hook.js` → `content.js` | fetch/XHR text responses | `rememberHookedText` |
| Dynamic src | `media-hook.js` → `content.js` | src setter, MutationObserver, XHR.open | `rememberHookedUrls` |
| Performance | `media.js` `collectPerformanceMediaItems` | Inside DOM scan | Resource timing URLs |

**Merge point:** `content.js` `collectPageMedia()` = `mergeMediaItems(collectMediaItems, collectDomVideoItems, hookedMediaItems)`.

**Background merge:** `mergeMediaItems(domItems, networkCapture.getForTab(tabId))`.

---

## URL classification contract

### `classifyMediaUrl(url)` returns `null` or `{ url, kind }`

**Reject when:**

- `blob:`, `data:`, `javascript:` (`isFetchable`)
- `PREVIEW_URL_RE` match (`getVideoPreview`, `getPreview`, `/preview/`)

**Accept when (in order):**

1. `HLS_URL_RE` → `kind: "hls"`
2. `DASH_URL_RE` → `kind: "dash"`
3. `MEDIA_EXT` on URL → `kind: "direct"`
4. `looksLikeSignedMediaUrl` (`sig`+`expires` or `type`+`sig`) → kind from `classifyStreamKind`
5. Query `mime=video` or `mime=audio` → `kind: "direct"`
6. `NETWORK_STREAM_RE` + stream path heuristics → kind from `classifyStreamKind`

### `classifyCapturedRequest(url, responseHeaders)`

Uses `Content-Type`, `Content-Disposition`, `Content-Length`, and custom filename headers. Adds `filename` and `size` via `withCapturedMetadata`.

Special cases:

- HLS/DASH MIME types
- `video/x-matroska`, `application/x-matroska`, `video/webm`
- `video/*`, `audio/*`
- `image/*` only if URL also matches `MEDIA_EXT`
- `application/octet-stream` ≥ 50_000 bytes if URL classifies

### `shouldListMediaItem(item)`

- `hls`, `dash`: always list
- `direct`: list unless `HLS_SEGMENT_RE` (`.ts`, `.m4s`, `.cmfv`, `.cmfa`, `.aac`, `.vtt`, `.key`)
- other kinds: do not list

### `classifyMediaCategory(item)` sort order

`video:0, audio:1, image:2, binary:3` — default popup sort uses this then filename.

---

## Page hook contracts

### `page-response-hook.js`

- Single install guard: `window.__avarResponseHookInstalled`
- Gate body scan on `MEDIA_HINT_RE` (extensions, stream paths, sig/expires)
- Only scan responses with content-type matching `json|text/plain|javascript|text/html`
- Post `{ type: "avar-response-body", text }` via `postMessage`

### `media-hook.js`

- Single install guard: `window.__avarMediaHookInstalled`
- Gate on `MEDIA_URL_HINT_RE` (narrower than full classification)
- Post `{ type: "avar-media-urls", urls: string[] }`
- Hook `HTMLMediaElement` and `HTMLSourceElement` `src` property + `setAttribute("src")`
- `MutationObserver` on `src` attribute, subtree
- Also hook `XMLHttpRequest.open` for capturable URLs

**Do not** duplicate full `classifyMediaUrl` in page hooks.

---

## Selection behavior

### `collectSelectedLinkItems(doc)`

- Requires non-collapsed selection with ranges
- Collects: anchors intersecting range, anchors in cloned fragment, bare URLs in `range.toString()` via `SELECTED_TEXT_URL_RE`
- Classifies each URL; unclassified URLs still included as `{ url, kind: classifyStreamKind(url) || "direct" }`

### Caching (content.js)

| Cache | TTL | Purpose |
|-------|-----|---------|
| `cachedSelectedItems` | 60 s | Context menu fallback |
| `contextMenuSnapshot` | 30 s | Snapshot on `contextmenu` event |

### Selection widget

- Show when: `showSelectionWidget !== false`, `items.length >= 1`, not dismissed
- Reset dismiss/manual position on selection key change
- Debounce: `SELECTION_WIDGET_DEBOUNCE_MS` = 100
- Bridge refresh: `BRIDGE_STATUS_REFRESH_MS` = 5000
- Opacity clamp: 40–100
- Download sends `avar-open-downloads` with `buildContentDownloadItem` payload (includes `referer: location.href`)

---

## Popup behavior contract

### On open

1. `loadConfig()` from storage
2. `scanPage()` → `avar-list-media` → render lists
3. `refreshBridgeStatus()` every 5 s
4. `startSelectionRefresh()` every 400 ms (selected links only)
5. HLS expansion when `bridgeConnected` and hls items exist

### Visible media list

`getVisibleMediaItems` = filter `shouldListMediaItem` → filter by type → sort → exclude URLs already in selected list.

### Size probe

- Only when `bridgeConnected`, `kind !== hls|dash`, size unknown
- Via `avar-probe-size` → bridge `url.probe`
- Results cached in `probedSizes` / `probedFilenames` Maps per popup session

### HLS expansion

- `avar-expand-hls-items` → bridge `hls.list` per hls item
- If >1 variant: replace with multiple rows (`hlsLabel`, `hlsResolution`, `masterUrl`)
- If 1 variant: update URL in place
- On failure: keep original playlist URL
- `hlsExpandGeneration` cancels stale async results

### Download actions

| UI action | Runtime message | Bridge type |
|-----------|-----------------|-------------|
| Per-item download | `avar-open-add-download` | `download.add.open` |
| Download all selected | `avar-open-batch-add` or `avar-open-downloads` | `download.batch.open` |
| Download all on page | `avar-open-batch-add` | `download.batch.open` |

Batch item shape (`buildBatchItem`):

```json
{
  "url", "streamKind", "filename", "linkName", "fileType",
  "fileSize", "originalUrl", "referer"
}
```

---

## Download intercept contract (`download-intercept.js`)

When `grabAllDownloads !== false`:

1. On `downloads.onCreated`
2. Skip non-http(s)/ftp, blob, extension schemes
3. Dedupe: same URL within 2000 ms
4. If `blockBrowserDownloads !== false`: `downloads.cancel`
5. If bridge reachable: `openSingleAdd` with url, streamKind, filename, referer

Duplicate within 2 s: still cancel if blocking, but do not re-forward.

---

## Bridge protocol (v1)

Envelope: `{ protocol: "avar.extension", version: 1, type, id, payload }`.

| type | Used by extension for |
|------|----------------------|
| `ping` | Startup registration, health |
| `status` | (available, rarely used) |
| `download.add` | Legacy direct queue |
| `download.add.open` | Single review dialog |
| `download.batch.open` | Batch review dialog |
| `url.probe` | Size/filename probe |
| `hls.list` | Master playlist variants |
| `queue.list` | Settings queue picker |
| `queue.start` / `queue.stop` | Queue control |

Endpoints: `GET /v1/ping`, `POST /v1`. Legacy: `GET /extension/ping`, `POST /extension/download`.

`ensureBridgeReachable` before dialog opens; `wakeAvarApp` via `avar://focus` background tab.

---

## Context menu contract

Menus recreated on `runtime.onInstalled`. `onShown` (when available) re-queries `collectFromTab(..., { forContextMenu: true })`.

| ID | Click handler |
|----|---------------|
| `avar-download-link` | `openLinkDownloadDialog(linkUrl)` |
| `avar-download-all-sub` | `openBatchAllMedia` — listed items only (`shouldListMediaItem`) |
| `avar-download-selected` | `openBatchSelectedMedia` — uses context menu snapshot path |

---

## Storage defaults (must match popup + content)

```javascript
showSelectionWidget: true          // !== false check
selectionWidgetOpacity: 100        // clamp 40-100
selectionWidgetTheme: "dark"       // only "light" or "dark"
selectedLinksInSeparateTab: true   // !== false check
grabAllDownloads: true             // !== false check
blockBrowserDownloads: true        // !== false check; disabled when grab off
defaultMediaFilter: "all"
mediaSort: "type"
bridgeUrl: "http://127.0.0.1:18766"
```

---

## Filename display priority (`itemDisplayFilename`)

1. `item.filename` (from headers/probe)
2. `guessFilenameFromUrl` if `isInferableUrlFilename`
3. Sanitized `pageTitle`
4. Raw URL segment or full URL

---

## Adding new detection (checklist)

1. Would it help on **multiple unrelated sites** without naming any?
2. Classification logic in `media.js`, not page hooks
3. Update `shared/`; list new injected scripts in manifest `web_accessible_resources`
4. Add to `syncSharedAssets` file list if new shared file
5. Update this file and `docs/extensions/behavior.md`
6. Manual test per architecture.md checklist

---

## Files map (quick reference)

| Concern | File |
|---------|------|
| URL regex / classify | `shared/media.js` |
| Page merge / widget | `shared/content.js` |
| Network capture | `shared/capture.js` |
| fetch/XHR body scan | `shared/page-response-hook.js` |
| Dynamic player src | `shared/media-hook.js` |
| HLS expand | `shared/hls.js` |
| Bridge HTTP | `shared/protocol.js` |
| Popup UI | `shared/popup.js`, `popup.html` |
| Context menus | `shared/context-menu.js` |
| Download grab | `shared/download-intercept.js` |
| Orchestration | `chromium/background.js`, `firefox/background.js` |
| Bridge server | `gui/electron/extension-bridge.cjs` |
