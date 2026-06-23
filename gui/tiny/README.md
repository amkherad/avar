# Tiny desktop shell (experiment)

Minimal desktop wrapper using [tinytron](https://github.com/Rafi993/tiny) (webview + basic window management). Goal: explore a ~10MB executable vs the full Electron bundle.

This shell runs the **same** Avar GUI SPA as Electron, but without the Electron preload bridge (`window.avar`). Behavior matches the **web** build: use session settings and enable **Use dev proxy** during development.

## Prerequisites

- Node.js 20+ (tinytron may need an older LTS if native build fails on your Node version)
- C++ build toolchain:
  - **Windows:** Visual Studio with "Desktop development with C++"
  - **macOS:** Xcode Command Line Tools
  - **Linux:** `build-essential`, `libwebkit2gtk-4.1-dev` (or distro equivalent for webview)

Install the native addon (optional — `npm install` in `gui/` continues if this fails):

```bash
cd gui
npm install tinytron
```

Upstream project: https://github.com/Rafi993/tiny

## Commands

```bash
cd gui
npm run dev:tiny      # Vite dev server + Tiny webview
npm run start:tiny    # Production build + static server + Tiny webview
```

Electron workflows are unchanged:

```bash
npm run dev:desktop
npm run build:desktop
```

## Architecture

Shared desktop code lives in `gui/desktop/`:

| Module | Purpose |
|--------|---------|
| `env.cjs` | Daemon URL, proxy port, window defaults |
| `daemon-proxy.cjs` | Local HTTP proxy to the daemon |
| `resolve-gui-url.cjs` | Dev / bundled / file URL resolution |
| `static-server.cjs` | Serves `dist/` for Tiny production mode |
| `shells.cjs` | Shell capability registry (Electron vs Tiny) |

`gui/electron/` and `gui/tiny/` are parallel entry points; neither replaces the other.

## Limitations (Tiny vs Electron)

Tiny does **not** provide:

- System tray
- Native popups / directory picker
- Desktop notifications via main process
- Browser extension bridge
- `window.avar` preload API

For full desktop features, use Electron (`npm run dev:desktop`).

CI builds the native addon with `npm run build:tiny` (see `.github/workflows/ci.yml`).

## Size experiment notes

tinytron embeds [webview](https://github.com/webview/webview) instead of Chromium. Packaging with [pkg](https://github.com/vercel/pkg) (as suggested upstream) is a follow-up step — this example focuses on running the GUI in a minimal shell first.
