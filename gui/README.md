# Avar GUI

React single-page application for the Avar download manager, packaged as an Electron desktop app. Connects to the daemon over HTTP JSON-RPC.

## Prerequisites

- Node.js 20+
- Avar daemon with HTTP channel enabled (default port `8000`):

```bash
avar daemon start --http --port=8000
```

## Development

```bash
cd gui
npm install
npm run dev          # http://localhost:56000 (proxies /api to daemon)
npm run dev:desktop  # Electron + Vite dev server
```

## Production builds

```bash
npm run build              # Static files in dist/ (bundled into Electron)
npm run build:desktop      # Electron installers for macOS, Windows, and Linux
npm run build:desktop:current  # Unpacked Electron bundle for the current OS (gui/release/)
```

Desktop installers and unpacked bundles are written to `gui/release/`.

### Web hosting

You can also serve `dist/` as static files from any web server. Configure the GUI session with the daemon base URL (e.g. `http://your-host:8000`). For local development, enable **Use dev proxy** in session settings so requests go through Vite's `/api` proxy.

Cross-origin browser access is enabled by default (`daemon.server.cors.enabled: true`, `allowOrigin: "*"`). Disable or restrict origins in `config.json` for production.

## Features

- **Sessions** — connect to multiple daemon servers; switch active session
- **Queues** — list queues and running state
- **Downloads** — per-queue download list with progress; add new downloads
- **Theme** — light, dark, or system
- **i18n** — English and Persian (RTL)

GUI preferences (theme, locale, sessions) are stored in browser `localStorage`, separate from daemon `config.json`.
