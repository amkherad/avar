---
title: Extension Behavior
sort: 2
parent: Browser Extensions
---

# Extension Behavior

This page describes everything the **Avar Download Helper** extension does — how it finds media, how downloads are sent to Avar, and what each setting controls.

## Overview

The extension runs on every web page and collects downloadable URLs through several **site-agnostic** techniques (no per-website rules). Detected URLs appear in the toolbar popup. When you choose to download, the extension opens Avar’s **Add download** or **Add downloads** dialog via a local HTTP bridge — it never talks to the Avar daemon directly.

**Requirements:** Avar Desktop (Electron) must be running with **Listen for browser extension connections** enabled. The bridge listens at `http://127.0.0.1:18766`.

## How media is detected

Detection layers run in parallel. Results are merged and deduplicated by URL.

### 1. DOM scan

On each page load and refresh, the content script scans:

| Source | What is collected |
|--------|-------------------|
| `<video>` / `<audio>` | `src`, `currentSrc`, nested `<source>` elements |
| `<a href>` | Links whose URL matches a known media extension |
| `<img src>` | Images with a media extension, or width/height ≥ 200 px |
| `<link>` | Preload links (`as=video` / `as=audio`) and links with media extensions |
| `data-src`, `data-url`, `data-hls`, `data-stream` | Attribute values resolved against the page base URL |
| Inline `<script>` text | Embedded stream URLs (regex extraction) |
| Page HTML | Same regex pass over `documentElement.innerHTML` |
| `performance.getEntriesByType("resource")` | Resource timing entries that classify as media |

**Video elements** use a separate pass (`collectDomVideoItems`) that also accepts player-assigned sources without file extensions — for example signed CDN URLs on `<video>` nodes.

### 2. Network observation

The background script listens to `webRequest.onCompleted` for all tabs. Each completed request is classified from its URL and response headers (`Content-Type`, `Content-Disposition`, `Content-Length`, and custom `x-filename` headers).

Captured URLs are stored per tab and cleared when the tab navigates or closes.

### 3. API response scan (page world)

A page-world script wraps `fetch` and `XMLHttpRequest`. When a response body looks like it might contain media URLs (JSON, HTML, plain text with hints like `.m3u8`, `videoplayback`, `sig=`), the text is posted to the content script for URL extraction.

### 4. Dynamic player hooks (page world)

Another page-world script:

- Hooks `HTMLMediaElement` / `HTMLSourceElement` `src` setter and `setAttribute("src", …)`
- Watches DOM mutations on `src` attributes and new `video`/`audio` nodes
- Intercepts `XMLHttpRequest.open` for capturable URLs

Matching URLs are posted to the content script immediately when players assign sources dynamically.

### What counts as media

Classification uses **generic signals only** — never hostnames or site-specific CSS:

| Signal | Examples |
|--------|----------|
| File extension | `.mp4`, `.webm`, `.mkv`, `.m3u8`, `.mpd`, `.mp3`, `.zip`, … |
| Stream path | `/hls/`, `/dash/`, `videoplayback`, `hls_playlist` |
| Signed CDN query | `sig` + `expires`, or `type` + `sig` (any host) |
| MIME in query string | `mime=video`, `mime=audio` |
| Response `Content-Type` | `video/*`, `audio/*`, HLS/DASH MIME types, `video/x-matroska` |
| Large `application/octet-stream` | ≥ 50 KB when URL also matches media heuristics |

**Excluded from the popup list:**

- `blob:`, `data:`, and `javascript:` URLs
- Preview URLs (`getVideoPreview`, `getPreview`, `/preview/`)
- HLS segment files (`.ts`, `.m4s`, `.cmfv`, etc.) — only playlists/manifests are listed

**Stream kinds:** Each item has a `kind`: `direct` (single file), `hls` (`.m3u8`), or `dash` (`.mpd`).

### HLS expansion

When HLS master playlists are found and the bridge is connected, the popup asks the bridge to list variants (`hls.list`). A single `.m3u8` entry may expand into multiple quality rows with resolution labels.

## Popup UI

Open the extension from the browser toolbar. On open, the popup scans the active tab and lists detected media.

### Main view

- **Connection status** — green dot = bridge reachable; red = Avar Desktop not running or bridge disabled.
- **Refresh** — re-scan the page.
- **Sort** — by type (video → audio → image → binary), by size ascending, or by size descending.
- **Filter** — All, Video, Audio, Image, or Binary.
- **Selected links** — URLs from the current text selection (anchors inside the selection or bare URLs in selected text).
- **Media list** — everything else detected on the page.
- **Download buttons** — open Avar’s review dialog (disabled when disconnected).

When **Put selected links in a separate tab** is enabled (default), Selected and Media appear as tabs with counts. Otherwise both sections stack in one scrollable list.

### Per-item row

Each row shows:

- Type icon (video, audio, image, binary)
- Display filename (from response headers, URL query/path, or page title as fallback)
- Stream badge (`hls` / `dash`) when applicable
- File size (from capture headers or a background probe via the bridge)
- Truncated URL with copy button
- Download button — opens **Add download** in Avar

**Size probing:** For direct files without a known size, the extension asks the bridge to HEAD/probe the URL (using the page as referer). HLS/DASH items show “—” for size.

### Bulk actions

| Action | Behavior |
|--------|----------|
| Download all selected | Opens **Add downloads** batch dialog with selected items |
| Download all on page | Opens batch dialog with all visible (filtered) media, excluding selected URLs |
| Per-item download | Opens **Add download** for one item; if multiple links are selected, per-item click opens batch instead |

Downloads are **not** queued immediately — Avar opens a review dialog where you confirm filename, queue, and options.

### Settings view

| Setting | Default | Effect |
|---------|---------|--------|
| Avar bridge URL | `http://127.0.0.1:18766` | HTTP endpoint for bridge messages. Only localhost port **18766** is accepted; other values fall back to the default. |
| Default download queue | (daemon default) | Pre-selects queue in Add download(s) dialogs |
| Default media type filter | All | Initial filter in the popup main view |
| Show selection widget | On | Floating on-page widget when text containing links is selected |
| Selection widget opacity | 100% | 40–100% |
| Selection widget theme | Dark | Dark or light widget chrome |
| Put selected links in a separate tab | On | Tabbed Selected/Media layout in popup |
| Grab all browser downloads | On | Intercept native browser downloads |
| Prevent the browser from saving files | On (requires Grab) | Cancel browser download; open Avar Add download instead |

Settings auto-save on change. Popup width is resizable (320–760 px); height resize is enabled only when the browser honors popup height changes.

## Selection widget

When you select text on a page that contains one or more links, a small floating widget appears near the selection:

- Shows link count and **Download all N file(s)**
- Draggable via the handle; position is remembered until selection changes
- Dismissible with × (reappears on new selection)
- Disabled when bridge is unreachable (“Avar was not found.”)

Controlled by **Show selection widget**, opacity, and theme settings. The content script debounces selection updates (100 ms) and refreshes bridge status every 5 seconds.

## Context menu

Right-click on a page or link:

| Menu item | When available | Action |
|-----------|----------------|--------|
| **Download with Avar** | Link context | Opens Add download for the link URL |
| **Avar integration → Download selected item(s)** | Page context, when selection contains links | Batch dialog for selected URLs |
| **Avar integration → Download all media** | Page context | Batch dialog for all listed media on the page |

The “Download selected” item is enabled only when the content script reports selected links. A snapshot is taken on `contextmenu` (30 s TTL) so the menu reflects the selection even if it changes slightly before click.

## Browser download interception

When **Grab all browser downloads** is enabled, the background script listens to `downloads.onCreated`:

1. If the URL is `http`, `https`, or `ftp` (not `blob:` / extension schemes), and not a duplicate within 2 seconds
2. Optionally cancels the browser download (**Prevent the browser from saving files**)
3. Opens Avar **Add download** with the URL, referer, and suggested filename

Requires a reachable bridge. Duplicates within 2 s are still cancelled if blocking is enabled, but not re-sent to Avar.

## Wake / focus behavior

Before opening a download dialog, the extension:

1. Pings the bridge (`/v1/ping`)
2. If unreachable, opens `avar://focus` in a background tab (then closes it) to wake Avar Desktop
3. Retries ping up to 10 times with backoff

This brings Avar to the foreground when downloads are requested.

## Privacy

- The extension only communicates with `127.0.0.1` / `localhost` on port **18766**
- Media URLs and page referers are sent to your local Avar bridge only
- No data is sent to external servers by the extension itself
- Page-world hooks run in the page context but only post URLs matching generic media patterns

## Limitations

- **Electron required** — the extension bridge runs inside Avar Desktop, not the web-only GUI or Vite dev server
- **No automatic queueing** — every download goes through Avar’s review dialog first (except legacy `download.add` direct queue, used internally for download intercept)
- **Site-agnostic only** — sites with heavily obfuscated players may need playback to start before network/DOM hooks capture URLs
- **HLS segments hidden** — individual `.ts` chunks are filtered out; download the playlist instead
- **Firefox temporary add-ons** — must be reloaded after browser restart when installed unpacked
