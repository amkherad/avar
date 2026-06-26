---
title: Usage
sort: 2
parent: Browser Extensions
---

# Using the Extension

Quick start for the **Avar Download Helper**. For complete behavior details, see [Extension Behavior]({{ site.baseurl }}/extensions/behavior.html).

## Prerequisites

1. **Avar Desktop** (Electron) running
2. **Settings → Browser integration → Listen for browser extension connections** enabled
3. Extension installed and showing a **green** connection dot

The bridge URL is `http://127.0.0.1:18766` (auto-detected).

## Capturing media

Open any web page, then click the extension icon. The popup scans the active tab and lists detected URLs.

Detection is automatic and **site-agnostic** — the extension uses MIME types, file extensions, streaming patterns, network headers, and dynamic player hooks. No per-website rules.

After opening a media-heavy page, start playback if URLs do not appear immediately.

## Downloading

| Method | How |
|--------|-----|
| **Popup — per item** | Click the download icon on a row → **Add download** dialog in Avar |
| **Popup — bulk** | **Download all selected** or **Download all on page** → **Add downloads** batch dialog |
| **Selection widget** | Select text containing links → floating **Download all** button on the page |
| **Context menu** | Right-click link → **Download with Avar**; or page → **Avar integration** submenu |
| **Browser downloads** | Enable **Grab all browser downloads** in extension Settings |

Downloads are always reviewed in Avar before queuing — the extension opens a dialog, not a silent queue (except when intercepting native browser downloads).

## Popup features

- **Filter** by type: All, Video, Audio, Image, Binary
- **Sort** by type, size ascending, or size descending
- **Selected links** tab — URLs from your text selection
- **Copy link** button on each row
- **Refresh** to re-scan the page
- **Settings** (gear icon) — bridge URL, default queue, widget options, download interception

## Connection status

| Indicator | Meaning |
|-----------|---------|
| Green dot | Bridge reachable; download buttons enabled |
| Red dot | Avar Desktop not running, bridge disabled, or wrong URL |

Troubleshooting:

1. Start or focus Avar Desktop
2. Enable **Listen for browser extension connections** in GUI settings
3. Confirm bridge URL is `http://127.0.0.1:18766` in extension Settings
4. Check the puzzle-piece icon in the Avar header for integration status

## Where downloads go

The **Add download(s)** dialog lets you pick the target queue and filename. Set a **Default download queue** in extension Settings to pre-select a queue.

## Privacy

The extension only talks to `127.0.0.1:18766`. Media URLs are forwarded to your local Avar instance only.

## See also

- [Extension Behavior]({{ site.baseurl }}/extensions/behavior.html) — full feature reference
- [Architecture]({{ site.baseurl }}/extensions/architecture.html) — technical overview
- [Protocol]({{ site.baseurl }}/extensions/protocol.html) — bridge message format
