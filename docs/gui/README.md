---
title: GUI
sort: 4
---

# GUI

The Avar GUI is a React single-page application that connects to the daemon over HTTP JSON-RPC. It runs in the browser, as an Electron desktop app, or embedded inside `avar-gui`.

## In this section

| Page | Contents |
|------|----------|
| [Overview]({{ site.baseurl }}/gui/overview.html) | Layout, navigation, and main areas |
| [Sessions]({{ site.baseurl }}/gui/sessions.html) | Connecting to daemon servers |
| [Downloads]({{ site.baseurl }}/gui/downloads.html) | Adding, searching, and controlling downloads |
| [Queues]({{ site.baseurl }}/gui/queues.html) | Sidebar queues and management |
| [Settings]({{ site.baseurl }}/gui/settings.html) | Theme, sync, proxy, daemon, and browser integration |
| [Layout]({{ site.baseurl }}/gui/layout.html) | Resizable panels and detail view |
| [Console]({{ site.baseurl }}/gui/console.html) | Log viewer drawer |
| [Shortcuts]({{ site.baseurl }}/gui/shortcuts.html) | Keyboard shortcuts |
| [Building]({{ site.baseurl }}/gui/building.html) | Development, production, and Electron builds |

## Prerequisites

The daemon must be running with HTTP enabled:

```bash
avar daemon start --http --port=8000
```

## Running the GUI

| Mode | Command | URL |
|------|---------|-----|
| **Web dev** | `cd gui && npm run dev` | `http://localhost:56821` |
| **Web production** | `npm run build` → serve `gui/dist/` | Your host + daemon URL in session settings |
| **Electron dev** | `npm run dev:desktop` | Desktop window |
| **Electron production** | `npm run build:desktop` | Installers in `gui/release/` |
| **Embedded** | Build `avar-gui` | `http://localhost:8000` |

GUI preferences (theme, locale, sessions) are stored in browser `localStorage`, separate from daemon `config.json`.

## Features at a glance

- Multiple daemon **sessions** with connection status
- **Queue** sidebar with start/stop and management
- **Download** list with card/table views, search, batch actions, and detail panel
- Live sync via polling, SSE, or WebSocket
- Light / dark / system **theme**
- English and Persian (**RTL**) **i18n**
- Desktop **notifications** and system **tray** (Electron)
- **PWA** install option in the browser
- **Browser extension** integration panel
