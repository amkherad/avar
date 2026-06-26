---
title: Extension Installation
sort: 1
parent: Browser Extensions
---

# Installing the Extension

## From the GUI (recommended)

1. Start the Avar daemon with HTTP enabled:
   ```bash
   avar daemon start --http --port=8000
   ```
2. Open **Avar Desktop** (Electron).
3. Go to **Settings → Browser integration**.
4. Enable **Listen for browser extension connections**.
5. Click the icon for your browser (Chrome, Firefox, Edge, or Opera) to download the extension package.
6. Extract the ZIP file.
7. Load the extension:
   - **Chrome / Edge / Opera:** Open `chrome://extensions`, enable **Developer mode**, click **Load unpacked**, select the extracted `chromium/` folder.
   - **Firefox:** Open `about:debugging`, click **This Firefox**, **Load Temporary Add-on**, select `manifest.json` from the extracted `firefox/` folder.

## Manual install (development)

If you have the repository cloned:

1. Start Avar Desktop with the daemon running.
2. Load unpacked directly from the source tree:
   - Chromium: `extensions/chromium/`
   - Firefox: `extensions/firefox/`

After editing `extensions/shared/`, run a GUI build or `npm run dev` in `gui/` to sync shared files into both browser folders.

## Verify the connection

After installation:

1. Click the extension icon in the browser toolbar.
2. The popup shows **Connected** with a green dot when the bridge is reachable.
3. In Avar, the puzzle-piece icon in the header (or **Settings → Browser integration**) should also show a green status.

## Bridge URL

The extension connects to the **Electron extension bridge**:

| Context | Bridge URL |
|---------|------------|
| Avar Desktop (default) | `http://127.0.0.1:18766` |

The extension auto-detects this URL. If connection fails, copy the **bridge URL** from **Settings → Browser integration** and paste it in the extension Settings view.

**Note:** The extension requires Avar Desktop. The web-only GUI and Vite dev server do not host the extension bridge.

## Permissions

The extension requests these permissions:

| Permission | Purpose |
|------------|---------|
| `activeTab` | Access the current tab for media detection |
| `storage` | Store bridge URL and preferences |
| `contextMenus` | Right-click download actions |
| `webRequest` | Observe network requests for media URLs |
| `downloads` | Intercept native browser downloads (optional) |
| `tabs` | Wake Avar via `avar://focus`; query active tab |
| `scripting` (Chromium) | Inject page-world detection scripts |
| `<all_urls>` | Scan pages on any site |
| localhost access | Communicate with the Avar bridge |

## Updating

When a new extension version is available, the GUI extension panel shows an update notice. Download the new package from **Settings → Browser integration** and reload the unpacked extension in your browser.

Extension source is maintained in `extensions/shared/` and synced to `chromium/` and `firefox/`.

## Firefox note

Temporary add-ons loaded via `about:debugging` are removed when Firefox restarts. Re-load the extension after each browser restart during development.
