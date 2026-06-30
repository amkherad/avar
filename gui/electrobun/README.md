# Electrobun desktop shell

Desktop wrapper using [Electrobun](https://electrobun.dev/) (Bun main process + native system webview). Runs the **same** Avar GUI SPA as Electron with full `window.avar` bridge parity.

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

| Component | Purpose |
|-----------|---------|
| `src/bun/index.ts` | Main process: windows, tray, extension bridge, daemon proxy |
| `src/views/avar-preload.ts` | Preload RPC bridge → `window.avar` (same API as Electron) |
| `shared/avar-rpc.ts` | Typed RPC contract between Bun and the webview |
| `src/bun/desktop-shell.ts` | Daemon HTTP proxy and GUI URL resolution |
| `electrobun.config.ts` | Bundles `gui/dist/`, extension bridge modules, preload view |

Shared desktop logic is **reused** from `gui/electron/` (extension bridge, protocol) and `gui/desktop/` via copy into the app bundle — Electron sources are not modified.

The Bun entrypoint must be `src/bun/index.ts` (outputs `app/bun/index.js`).

## Feature parity (Electrobun vs Electron)

| Feature | Electrobun |
|---------|------------|
| `window.avar` preload API | Yes (RPC) |
| Daemon HTTP proxy (`getProxyBaseUrl`) | Yes |
| Browser extension bridge | Yes |
| System tray + bulk download actions | Yes |
| Native popups (add download / batch) | Yes |
| Directory picker | Yes |
| Desktop notifications | Yes |
| Open file / show in folder / external URLs | Yes |
| Remote download file save | Yes |
| `avar://` deep links | macOS (Electrobun `urlSchemes`); Windows/Linux when OS registration is available |

Packaged artifacts are written under `gui/electrobun/artifacts/` (Electrobun default).
