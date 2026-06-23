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
2. Open the Avar GUI or Electron desktop app.
3. Go to **Settings → Browser integration**.
4. Enable **Listen for browser extension connections**.
5. Click the icon for your browser (Chrome, Firefox, Edge, or Opera) to download the extension package.
6. Extract the ZIP file.
7. Load the extension:
   - **Chrome / Edge / Opera:** Open `chrome://extensions`, enable **Developer mode**, click **Load unpacked**, select the extracted `chromium/` folder.
   - **Firefox:** Open `about:debugging`, click **This Firefox**, **Load Temporary Add-on**, select `manifest.json` from the extracted `firefox/` folder.

## Manual install (development)

If you have the repository cloned:

1. Start the daemon: `avar daemon start --http --port=8000`
2. Load unpacked directly from the source tree:
   - Chromium: `extensions/chromium/`
   - Firefox: `extensions/firefox/`

## Verify the connection

After installation:

1. Click the extension icon in the browser toolbar.
2. The popup shows a **green dot** when the bridge is reachable, **red** when it is not.
3. In the GUI, the extension status indicator in **Settings → Browser integration** should also turn green.

## Bridge URLs

The extension auto-detects the bridge based on what is running:

| Context | Bridge URL |
|---------|------------|
| Electron desktop | `http://127.0.0.1:18766` (auto-detected) |
| Web dev (`npm run dev`) | `http://127.0.0.1:56821` (fallback) |
| Custom | Paste the URL from **Settings → Browser integration** |

If auto-detection fails, copy the **bridge URL** from the GUI settings and paste it in the extension popup.

## Permissions

The extension requests these permissions:

| Permission | Purpose |
|------------|---------|
| `activeTab` | Access the current tab for media detection |
| `storage` | Store bridge URL and preferences |
| `contextMenus` | Right-click "Download with Avar" actions |
| `scripting` / background scripts | Inject media detection helpers |
| `webRequest` | Observe network requests for media URLs |
| `<all_urls>` | Scan pages on any site |
| localhost access | Communicate with the Avar bridge |

## Updating

When a new extension version is available, the GUI extension panel shows an update notice. Download the new package from **Settings → Browser integration** and reload the unpacked extension in your browser.

Extension source is maintained in `extensions/shared/` and synced to `chromium/` and `firefox/`.
