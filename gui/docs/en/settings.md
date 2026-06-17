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
- **Footer monitors** — Toggle disk, memory, CPU, and network stats in the footer (collected from the daemon when connected).

### ⌨️ Shortcuts

View and remap keyboard shortcuts in a single aligned table. Click a shortcut button, then press the new key combination. Press **Escape** to cancel recording.

Use **Reset all** to restore defaults.

Settings (theme, language, server labels/URLs, and monitor toggles) are stored locally in the browser or Electron profile (`localStorage`). Auth tokens are kept separately.
