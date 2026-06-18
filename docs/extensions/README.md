---
title: Browser Extensions
sort: 5
---

# Browser Extensions

The **Avar Download Helper** browser extension captures media links from web pages and sends them to the Avar download manager.

## In this section

| Page | Contents |
|------|----------|
| [Installation]({{ site.baseurl }}/extensions/installation.html) | Install from the GUI or load unpacked |
| [Usage]({{ site.baseurl }}/extensions/usage.html) | Capturing media and queuing downloads |
| [Protocol]({{ site.baseurl }}/extensions/protocol.html) | Bridge messaging format |

## What it does

The extension scans web pages for downloadable media using **site-agnostic detection** — it looks for media by MIME type, file extension, and streaming patterns (HLS/DASH) without per-website extractors.

Capabilities:

- Detect video, audio, image, and archive URLs on any page
- Context menu actions to download linked media
- Popup UI showing bridge connection status
- Forward URLs to Avar via a local **bridge** → daemon `download.add` JSON-RPC

## Supported browsers

| Browser | Extension folder | Manifest |
|---------|------------------|----------|
| Chrome, Edge, Opera | `extensions/chromium/` | Manifest V3 |
| Firefox | `extensions/firefox/` | Manifest V2 |

Source code: [{{ site.repo_url }}/tree/main/extensions]({{ site.repo_url }}/tree/main/extensions)

## Requirements

1. Avar daemon running with HTTP enabled
2. GUI or Electron app running (for the extension bridge), **or** the Vite dev server during development
3. Extension installed and connected (green status indicator)

## Quick install

1. Start the daemon: `avar daemon start --http --port=8000`
2. Open the GUI → **Settings → Browser integration**
3. Download the extension ZIP for your browser
4. Load unpacked from `extensions/chromium/` or `extensions/firefox/`

See [Installation]({{ site.baseurl }}/extensions/installation.html) for detailed steps.
