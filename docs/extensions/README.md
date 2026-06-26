---
title: Browser Extensions
sort: 5
---

# Browser Extensions

The **Avar Download Helper** browser extension captures media links from web pages and sends them to the Avar download manager through a local bridge.

## In this section

| Page | Contents |
|------|----------|
| [Installation]({{ site.baseurl }}/extensions/installation.html) | Install from the GUI or load unpacked |
| [Usage]({{ site.baseurl }}/extensions/usage.html) | Quick start — capturing and queuing downloads |
| [Extension Behavior]({{ site.baseurl }}/extensions/behavior.html) | Complete behavior reference (detection, UI, settings) |
| [Protocol]({{ site.baseurl }}/extensions/protocol.html) | Bridge HTTP messaging format |
| [Architecture]({{ site.baseurl }}/extensions/architecture.html) | Code layout and technical overview |

## What it does

The extension scans web pages for downloadable media using **site-agnostic detection** — MIME types, file extensions, streaming patterns (HLS/DASH), network headers, and dynamic player hooks. It does not use per-website extractors or hostname checks.

Capabilities:

- Detect video, audio, image, and archive URLs on any page
- Floating selection widget for bulk-downloading selected links
- Context menu actions (link, selected, all media)
- Popup with filter, sort, size probe, and HLS variant expansion
- Optional interception of native browser downloads
- Forward URLs to Avar via Electron bridge → review dialog → daemon queue

## Supported browsers

| Browser | Extension folder | Manifest |
|---------|------------------|----------|
| Chrome, Edge, Opera | `extensions/chromium/` | Manifest V3 |
| Firefox | `extensions/firefox/` | Manifest V2 |

Source code: [{{ site.repo_url }}/tree/main/extensions]({{ site.repo_url }}/tree/main/extensions)

## Requirements

1. **Avar Desktop** (Electron) running
2. Daemon running with HTTP enabled
3. **Listen for browser extension connections** enabled in GUI settings
4. Extension installed with green connection indicator

## Quick install

1. Start the daemon: `avar daemon start --http --port=8000`
2. Open Avar Desktop
3. **Settings → Browser integration** → download extension for your browser
4. Load unpacked from `extensions/chromium/` or `extensions/firefox/`

See [Installation]({{ site.baseurl }}/extensions/installation.html) for detailed steps.

## For developers and agents

- Repository dev README: `extensions/README.md`
- Agent behavior contract: `.agents/skills/extension-development/BEHAVIOR.md`
- Agent skill: `.agents/skills/extension-development/SKILL.md`
