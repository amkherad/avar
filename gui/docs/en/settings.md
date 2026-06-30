# ⚙️ Settings

Open settings from the header gear icon. The main workspace (sidebar, downloads, footer) is hidden while settings are open.

## 📂 Categories

Use the inner sidebar to switch between setting groups.

### 🌐 General

- **Theme** — Light (muted surfaces, default for bright rooms), Light (bright), Dark, or follow system preference. System uses the muted light theme when your OS is in light mode.
- **Language** — English or Persian (فارسی). RTL layout applies automatically for Persian.
- **Data sync channel** — How the GUI receives live updates:
  - *Periodic polls* — interval configurable below.
  - *Server-sent events* — push updates over SSE.
  - *WebSockets* — bidirectional WebSocket channel.
- **Refresh interval** — Poll frequency when using periodic sync (seconds).
- **Connection check interval** — How often the GUI pings the daemon (seconds).
- **Double-click download** — When connected to a local daemon, choose what happens when you double-click a download row: open the completed file with your OS default app (Electron desktop only) or open the details popup window. Defaults to **Open completed file** when available.
- **Size units** — Show download sizes and footer disk/memory values in binary (KiB, MiB, …) or decimal (KB, MB, …). Default is binary.
- **Transfer rate units** — Show download speeds and footer network throughput in binary bytes per second (KiB/s, …) or binary bits per second (Kib/s, …). Default is KiB/s.
- **Remote downloads** — **Local download folder** used when you copy completed files from a remote daemon to your computer (see **Copy to local folder** in the downloads panel). Ignored for local sessions.
- **Footer monitors** — Toggle disk, memory, CPU, and network stats in the footer (collected from the daemon when connected). Choose **Text values** or **Histogram + values** to show sparkline-style history for memory, CPU, and network. Histogram mode keeps labels inline with semi-transparent values overlaid on the chart.
- **Notifications** — Toggle desktop notifications for download status changes, queue start/stop, and connection alerts. In the browser, you may still need to grant notification permission separately.
- **Keep in system tray when closing the window** (desktop app only) — When enabled (default), closing the main window hides Avar to the system tray. When disabled, closing the main window quits the application completely.
- **Install web app** — Install Avar as a standalone PWA (when your browser supports it).

### 🌐 Browser integration

Install the Avar browser extension to queue media from web pages through the Avar bridge:

1. Enable **Listen for browser extension connections** (disable to reject extension requests).
2. Use **Suspend extension integration** for a temporary pause without changing the listen preference (for example if the extension misbehaves). Resume when ready.
3. Check the status indicator: green when an installed extension can reach the bridge, red when it cannot.
4. Click your browser icon (Chrome, Firefox, Edge, or Opera) to download the extension package.
5. Extract the ZIP and load it as an unpacked extension (Developer mode).
6. The extension auto-detects the Electron bridge (`http://127.0.0.1:18766`). Paste the **bridge URL** from settings if auto-detection fails.

The extension popup shows a green/red dot for bridge connectivity. Use **Download selected**, **Download all**, or the per-item download button to open the **Add downloads** batch dialog in Avar (a separate window in Electron). Review each file, choose a target queue, then **Queue download** or **Queue and start**.

In the extension **Settings** view, enable **Grab all browser downloads** to send every native browser download to Avar and open the **Add download** dialog. Pair it with **Prevent the browser from saving files** to cancel the browser download and show the Avar dialog instead of the browser save prompt.

### 🖥️ Desktop tray (Electron)

When running the Electron app, Avar stays available in the system tray (Windows, macOS, KDE, GNOME, and other Linux desktops with tray support). Left-click the tray icon to show the main window; right-click for **Show Avar**, up to three active downloads with progress (oldest first), bulk download actions (**Start All**, **Pause All**, **Resume All**, **Stop All**), and **Exit Avar**. The tray tooltip also summarizes active downloads. By default, closing the main window hides it to the tray instead of quitting. Turn off **Keep in system tray when closing the window** under **General** to quit when you close the main window.

Use the **puzzle-piece icon** in the header (next to the theme toggle) to open the extension integration panel: connection status, listen toggle, temporary suspend/resume, bridge URL, protocol version, and extension update availability.

### ⌨️ Shortcuts

View and remap keyboard shortcuts in a single aligned table. Click a shortcut button, then press the new key combination. Press **Escape** to cancel recording.

Use **Reset all** to restore defaults.

### 📥 Downloads

Configure default download paths, segmentation, progress display, and **proxy** settings. Parallel segments are **enabled by default** (up to four concurrent segments per download). New queues default to four parallel downloads unless you set a per-queue limit.

- **Enable parallel segments** — Split large files into concurrent range requests (on by default).
- **Max concurrent segments** — Parallel segments per download item (default: 4).

- **Use proxy** — Enable a global HTTP, HTTPS, or SOCKS5 proxy for downloads that do not specify their own proxy.
- **Host / Port / Username / Password** — Proxy server credentials. Stored in `config.json` under `dm.proxy.*`.
- **Bypass proxy for (NO_PROXY)** — Comma-separated hosts or domains that should connect directly (for example `localhost,127.0.0.1,.example.com`). Matches the `NO_PROXY` environment variable when set.

Per-download proxy overrides are available in **Add download**. Global proxy settings apply when a download has no override and when standard environment variables (`HTTP_PROXY`, `HTTPS_PROXY`, `ALL_PROXY`, `NO_PROXY`) are not set.

### Daemon

- **Auto shutdown** — Never, or when idle (no active downloads). When idle shutdown is enabled, set **Idle time before shutdown** in seconds.
- **File logging** — Optionally write daemon logs to a file path.
- **Remote file download** — When enabled, the daemon serves completed files at `GET /api/downloads/{id}/file`. Required for **Copy to local folder** on remote sessions. Disabled by default in `config.json` (`daemon.server.fileDownload.enabled`).
- **Remote directory browser** — When enabled (`daemon.server.fsBrowse.enabled`), the GUI can browse folders on the daemon when configuring paths on remote sessions. Disabled by default.

### ✨ About

The last item in the settings sidebar. Shows **front-end** and **back-end (daemon)** version numbers, author credits, open-source license link, sponsor invitation, support buttons (GitHub Sponsors and Star), and **Report a bug** (opens GitHub Issues).

Settings (theme, language, server labels/URLs, and monitor toggles) are stored locally in the browser or Electron profile (`localStorage`). Auth tokens are kept separately.
