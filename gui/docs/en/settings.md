# ⚙️ Settings

Open settings from the header gear icon. The main workspace (sidebar, downloads, footer) is hidden while settings are open.

## 📂 Categories

Use the inner sidebar to switch between setting groups.

### 🌐 General

- **Theme** — Light, Dark, or follow system preference.
- **Language** — English or Persian (فارسی). RTL layout applies automatically for Persian.
- **Data sync channel** — How the GUI receives live updates:
  - *Periodic polls* — interval configurable below.
  - *Server-sent events* — push updates over SSE.
  - *WebSockets* — bidirectional WebSocket channel.
- **Refresh interval** — Poll frequency when using periodic sync (seconds).
- **Connection check interval** — How often the GUI pings the daemon (seconds).
- **Remote downloads** — **Local download folder** used when you copy completed files from a remote daemon to your computer (see **Copy to local folder** in the downloads panel). Ignored for local sessions.
- **Footer monitors** — Toggle disk, memory, CPU, and network stats in the footer (collected from the daemon when connected). Choose **Text values** or **Histogram + values** to show sparkline-style history for memory, CPU, and network. Histogram mode keeps labels inline with semi-transparent values overlaid on the chart.
- **Notifications** — Toggle desktop notifications for download status changes, queue start/stop, and connection alerts. In the browser, you may still need to grant notification permission separately.
- **Install web app** — Install Avar as a standalone PWA (when your browser supports it).

### 🌐 Browser integration

Install the Avar browser extension to queue media from web pages through the Avar bridge:

1. Enable **Listen for browser extension connections** (disable to reject extension requests).
2. Check the status indicator: green when an installed extension can reach the bridge, red when it cannot.
3. Click your browser icon (Chrome, Firefox, Edge, or Opera) to download the extension package.
4. Extract the ZIP and load it as an unpacked extension (Developer mode).
5. The extension auto-detects the Electron bridge (`http://127.0.0.1:18766`). Paste the **bridge URL** from settings if auto-detection fails.

The extension popup shows a green/red dot for bridge connectivity. Downloads are forwarded from the extension to the bridge, then to the daemon.

### 🖥️ Desktop tray (Electron)

When running the Electron app, Avar stays available in the system tray (Windows, macOS, KDE, GNOME, and other Linux desktops with tray support). Left-click the tray icon to show the main window; right-click for **Show Avar**, up to three active downloads with progress (oldest first), bulk download actions (**Start All**, **Pause All**, **Resume All**, **Stop All**), and **Exit Avar**. The tray tooltip also summarizes active downloads. Closing the main window hides it to the tray instead of quitting.

Use the **puzzle-piece icon** in the header (next to the theme toggle) to open the extension integration panel: connection status, listen toggle, bridge URL, protocol version, and extension update availability.

### ⌨️ Shortcuts

View and remap keyboard shortcuts in a single aligned table. Click a shortcut button, then press the new key combination. Press **Escape** to cancel recording.

Use **Reset all** to restore defaults.

### 📥 Downloads

Configure default download paths, segmentation, progress display, and **proxy** settings:

- **Use proxy** — Enable a global HTTP, HTTPS, or SOCKS5 proxy for downloads that do not specify their own proxy.
- **Host / Port / Username / Password** — Proxy server credentials. Stored in `config.json` under `dm.proxy.*`.
- **Bypass proxy for (NO_PROXY)** — Comma-separated hosts or domains that should connect directly (for example `localhost,127.0.0.1,.example.com`). Matches the `NO_PROXY` environment variable when set.

Per-download proxy overrides are available in **Add download**. Global proxy settings apply when a download has no override and when standard environment variables (`HTTP_PROXY`, `HTTPS_PROXY`, `ALL_PROXY`, `NO_PROXY`) are not set.

### Daemon

- **Auto shutdown** — Never, or when idle (no active downloads). When idle shutdown is enabled, set **Idle time before shutdown** in seconds.
- **File logging** — Optionally write daemon logs to a file path.
- **Remote file download** — When enabled, the daemon serves completed files at `GET /api/downloads/{id}/file`. Required for **Copy to local folder** on remote sessions. Disabled by default in `config.json` (`daemon.server.fileDownload.enabled`).

Settings (theme, language, server labels/URLs, and monitor toggles) are stored locally in the browser or Electron profile (`localStorage`). Auth tokens are kept separately.
