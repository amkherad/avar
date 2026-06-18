---
title: Overview
sort: 1
parent: GUI
---

# GUI Overview

Welcome to **Avar** — a download manager with a web and desktop interface. The GUI connects to the Avar daemon over HTTP and lets you manage queues, downloads, and sessions from one place.

## Main areas

| Area | Location | Purpose |
|------|----------|---------|
| **Header** | Top bar | App branding, back navigation, theme, Help, and Settings |
| **Sidebar** | Left | Queues, help topics, or settings categories |
| **Session** | Bottom of sidebar | Active daemon session selector |
| **Downloads** | Center | Search, add, select, and control downloads |
| **Detail panel** | Right (optional) | Selected download metadata, curl copy, and actions |
| **Footer** | Bottom | Server health stats, optional system monitors, console and detail panel toggles |
| **Console** | Bottom drawer | GUI and daemon log output |

## Navigation

- Use the **back** arrow in the header to return to the dashboard from other pages.
- Click **Settings** (gear icon) to open application settings.
- Click **Help** (question icon) to open in-app documentation.
- Press **Ctrl+1** to return to the main dashboard from anywhere.
- Use **Manage** in the queue sidebar to open the queue management page.

## Getting started with the GUI

1. Ensure the Avar daemon is running with HTTP enabled (default `http://127.0.0.1:8000`).
2. Start the GUI — `npm run dev` for development or open the Electron app.
3. Select or add a **session** in the sidebar if you connect to a remote daemon.
4. Choose a **queue** and click **Add download** to start a new transfer.

## Web vs desktop

| | Web | Electron desktop |
|---|-----|------------------|
| **Install** | Open in browser or install as PWA | Run installer from `gui/release/` |
| **Daemon connection** | Configure session URL manually | Built-in Electron tunnel session |
| **System tray** | Not available | Minimize to tray, tray menu with active downloads |
| **Extension bridge** | Via Vite dev server or configured URL | Auto-detected at `http://127.0.0.1:18766` |
| **Notifications** | Browser permission required | Native desktop notifications |

See [Building]({{ site.baseurl }}/gui/building.html) for how to run and package each mode.
