# Electrobun desktop shell (experiment)

Minimal desktop wrapper using [Electrobun](https://electrobun.dev/) (Bun main process + native system webview). Goal: explore a small cross-platform desktop bundle as an alternative to Electron.

This shell runs the **same** Avar GUI SPA as Electron, but without the Electron preload bridge (`window.avar`). Behavior matches the **web** build: use session settings and enable **Use dev proxy** during development.

## Prerequisites

- Node.js 20+ (same as the main gui package)
- C++ build toolchain (Electrobun native CLI):
  - **Windows:** Visual Studio with "Desktop development with C++"
  - **macOS:** Xcode Command Line Tools
  - **Linux:** `build-essential`, `libwebkit2gtk-4.1-dev` (or distro equivalent)

Electrobun bundles Bun with packaged apps; you do **not** need Bun installed globally.

## Commands

```bash
cd gui
npm run dev:electrobun      # Vite dev server + Electrobun webview
npm run build:electrobun    # Vite build + Electrobun stable package
```

Electron workflows are unchanged:

```bash
npm run dev:desktop
npm run build:desktop
```

## Architecture

Shared desktop code lives in `gui/desktop/` (reused via `createRequire` from the Bun main process):

| Module | Purpose |
|--------|---------|
| `env.cjs` | Daemon URL, proxy port, window defaults |
| `daemon-proxy.cjs` | Local HTTP proxy to the daemon |
| `resolve-gui-url.cjs` | Dev / bundled / file URL resolution |
| `static-server.cjs` | Serves `dist/` when a local HTTP origin is required |

`gui/electrobun/` is a parallel entry point; it does not replace `gui/electron/` or `gui/tiny/`.

The Bun entrypoint must be `src/bun/index.ts` (outputs `app/bun/index.js`) — the Electrobun launcher does not load `main.js`.

## Limitations (Electrobun vs Electron)

Electrobun does **not** provide:

- System tray
- Native popups / directory picker
- Desktop notifications via main process
- Browser extension bridge
- `window.avar` preload API

For full desktop features, use Electron (`npm run dev:desktop`).

Packaged artifacts are written under `gui/electrobun/artifacts/` (Electrobun default).
