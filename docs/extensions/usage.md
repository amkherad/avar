---
title: Usage
sort: 2
parent: Browser Extensions
---

# Using the Extension

## Capturing media from a page

The extension scans the current page for downloadable media automatically. When media is found, you can queue it for download through several methods.

### Extension popup

1. Click the **Avar Download Helper** icon in the browser toolbar.
2. The popup lists detected media URLs on the current page.
3. Click a URL or use the download button to send it to Avar.
4. The download appears in the active queue in the GUI.

### Context menu

Right-click on a link, image, video, or audio element and choose **Download with Avar**. The extension sends the URL to the bridge immediately.

### Automatic detection

Content scripts monitor the page for:

- `<video>` and `<audio>` elements with `src` attributes
- Links to common media extensions (`.mp4`, `.webm`, `.mkv`, `.mp3`, `.flac`, `.zip`, etc.)
- HLS (`.m3u8`) and DASH (`.mpd`) manifest URLs
- Network requests matching media MIME types

Detection is **site-agnostic** — it does not use per-website rules or hostname checks.

## Connection status

| Indicator | Meaning |
|-----------|---------|
| Green dot | Bridge reachable; downloads will be forwarded |
| Red dot | Bridge unreachable; check daemon and GUI are running |

Troubleshooting a red indicator:

1. Confirm the daemon is running: `avar daemon ping`
2. Confirm the GUI or Electron app is open (the bridge runs inside it)
3. Check **Settings → Browser integration** — **Listen for browser extension connections** must be enabled
4. Manually set the bridge URL in the extension popup

## Where downloads go

Downloads are forwarded to the **active daemon session** configured in the GUI. They appear in the currently selected queue. Change the active queue in the GUI sidebar before sending URLs if you want them in a specific queue.

## Electron vs web

| Mode | Bridge | Notes |
|------|--------|-------|
| **Electron desktop** | `http://127.0.0.1:18766` | Auto-detected; most reliable |
| **Web GUI (dev)** | `http://127.0.0.1:5173` | Requires `npm run dev` |
| **Web GUI (production)** | Configured bridge URL | Set manually in extension popup |

## Privacy

The extension only communicates with localhost bridge URLs. It does not send data to external servers. Media URLs are forwarded to your local Avar daemon only.
