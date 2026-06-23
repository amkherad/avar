---
title: Building
sort: 9
parent: GUI
---

# Building the GUI

The Avar GUI is a React + Vite application with an optional Electron desktop shell.

Source code: [{{ site.repo_url }}/tree/main/gui]({{ site.repo_url }}/tree/main/gui)

## Prerequisites

- Node.js 20+ (CI uses Node 22)
- Avar daemon with HTTP enabled on port 8000

## Development

```bash
cd gui
npm install
npm run dev          # http://localhost:5173 — proxies /api to daemon
npm run dev:desktop  # Electron + Vite dev server
```

Enable **Use dev proxy** in session settings when using `npm run dev`.

## Production builds

```bash
npm run build              # Static files in gui/dist/
npm run build:desktop      # Electron installers for macOS, Windows, Linux
npm run build:desktop:current  # Package for the current OS only
```

### Web hosting

Serve `gui/dist/` as static files from any web server. Configure the GUI session with the daemon base URL (e.g. `http://your-host:8000`).

Cross-origin access is enabled by default in the daemon (`daemon.server.cors.enabled: true`). Restrict origins in `config.json` for production.

### Electron installers

`npm run build:desktop` produces platform-specific packages in `gui/release/`:

| Platform | Formats |
|----------|---------|
| macOS | DMG, ZIP |
| Windows | NSIS installer, portable |
| Linux | AppImage, deb |

## Embedded GUI (`avar-gui`)

Build the C binary with the GUI embedded:

```bash
cmake -S . -B output/build-gui -DCMAKE_BUILD_TYPE=Release -DAVAR_BUILD_GUI=ON -G Ninja
cmake --build output/build-gui --target avar-gui --parallel
```

The daemon serves the SPA on its HTTP port — no separate web server needed.

## Architecture notes

The GUI is a **pure SPA** — no server-side rendering. All daemon communication goes through HTTP JSON-RPC (`gui/src/api/daemon.ts`). GUI preferences live in `localStorage`, not in `config.json`.

## PWA

When hosted over HTTPS, users can install Avar as a Progressive Web App from **Settings → General → Install web app**.
