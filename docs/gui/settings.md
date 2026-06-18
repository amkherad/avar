---
title: Settings
sort: 5
parent: GUI
---

# Settings

Open settings from the header gear icon. The main workspace is hidden while settings are open.

## Categories

Use the inner sidebar to switch between setting groups: **General**, **Downloads**, **Queues**, **Daemon**, **Browser**, and **Shortcuts**.

### General

- **Theme** — Light, Dark, or follow system preference.
- **Language** — English or Persian (فارسی). RTL layout applies automatically for Persian.
- **Data sync channel** — How the GUI receives live updates:
  - *Periodic polls* — interval configurable below.
  - *Server-sent events* — push updates over SSE.
  - *WebSockets* — bidirectional WebSocket channel.
- **Refresh interval** — Poll frequency when using periodic sync (seconds).
- **Connection check interval** — How often the GUI pings the daemon (seconds).
- **Footer monitors** — Toggle disk, memory, CPU, and network stats in the footer.
- **Notifications** — Desktop notifications for download status changes, queue start/stop, and connection alerts.
- **Install web app** — Install Avar as a standalone PWA (when your browser supports it).

### Downloads

Configure default download paths, segmentation, progress display, and **proxy** settings:

- **Use proxy** — Enable a global HTTP, HTTPS, or SOCKS5 proxy.
- **Host / Port / Username / Password** — Proxy credentials (stored in `config.json` under `dm.proxy.*`).
- **Bypass proxy for (NO_PROXY)** — Comma-separated hosts that connect directly.

Per-download proxy overrides are available in **Add download**.

### Queues

Create, edit, and delete queues. See [Queues]({{ site.baseurl }}/gui/queues.html).

### Daemon

- **Auto shutdown** — Never, or when idle (no active downloads).
- **Idle time before shutdown** — Seconds of inactivity before auto shutdown.
- **File logging** — Optionally write daemon logs to a file path.

### Browser integration

Install the Avar browser extension to queue media from web pages:

1. Enable **Listen for browser extension connections**.
2. Check the status indicator: green when an extension can reach the bridge.
3. Click your browser icon (Chrome, Firefox, Edge, or Opera) to download the extension package.
4. Extract the ZIP and load it as an unpacked extension (Developer mode).
5. The extension auto-detects the Electron bridge. Paste the **bridge URL** if auto-detection fails.

See [Browser extensions]({{ site.baseurl }}/extensions/) for full installation instructions.

Use the **puzzle-piece icon** in the header to open the extension integration panel.

### Desktop tray (Electron)

When running the Electron app, Avar stays in the system tray. Left-click to show the main window; right-click for download controls and **Exit Avar**. Closing the main window hides it to the tray instead of quitting.

### Shortcuts

View and remap keyboard shortcuts. See [Shortcuts]({{ site.baseurl }}/gui/shortcuts.html).

## Where settings are stored

| Settings | Location |
|----------|----------|
| Theme, language, sessions, shortcuts | Browser `localStorage` (`avar.gui.config`) |
| Download paths, proxy, queues, daemon | Daemon `config.json` |
| Auth tokens | Stored separately per session |
