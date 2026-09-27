# Qt GUI reference (session-persisted)

## Repository paths

| Path | Purpose |
|------|---------|
| `gui-qt/` | Qt 6 application (`avar-gui-qt`) |
| `gui-qt/build/desktop/` | Native binary output |
| `gui-qt/build/wasm/` | `avar-gui-qt.html`, `.js`, `.wasm`, `qtloader.js` |
| `extensions/daemon/` | `avar-extension-daemon` (C HTTP bridge) |
| `.emsdk/` | Local Emscripten SDK (optional; not committed) |
| `.qt/` | Local Qt kits via aqtinstall (optional; not committed) |
| `gui/src/api/daemon.ts` | Authoritative daemon client API |
| `gui/src/theme/themes.ts` | Authoritative theme tokens |
| `gui/electron/extension-protocol.cjs` | Extension protocol constants |

## Environment variables

| Variable | Used for |
|----------|----------|
| `QT_ROOT` | Wasm Qt install, e.g. `.qt/6.4.2/wasm_32` |
| `QT_HOST_PATH` | Host Qt for cross-compile tools, e.g. `.qt/6.4.2/gcc_64` |
| `EMSDK` | Emscripten root; `source $EMSDK/emsdk_env.sh` before wasm build |
| `PORT` | `serve_wasm.sh` listen port (default 8765) |

`build_wasm.sh` derives `QT_HOST_PATH` from `QT_ROOT` when sibling `gcc_64` exists.

## CMake options (`gui-qt`)

| Option | Default | Meaning |
|--------|---------|---------|
| `AVAR_GUI_QT_WASM` | OFF | Wasm cross-build |
| `AVAR_GUI_QT_BUILD_TESTS` | ON | Qt Test targets |
| `AVAR_GUI_QT_ENABLE_EXTENSION_SUBPROCESS` | ON (desktop) | OFF on wasm script |

## Extension bridge endpoints

| Method | Path |
|--------|------|
| GET | `/v1/ping` |
| POST | `/v1` | Envelope (`avar.extension` v1) |
| POST | `/extension/settings` |
| GET | `/extension/status` |

Default bind: `127.0.0.1:18766` (`AVAR_EXTENSION_DEFAULT_PORT`).

## Parity score (baseline)

Run `parity_audit.py` for current numbers. Typical gap categories:

- Popup pages (`AddDownloadPopupPage`, …)
- Settings panels (only `general` fully implemented early on)
- Most `DaemonClient` methods vs `daemon.ts`
- Session selector, footer stats, console, shortcuts
- Stream snapshot parsing (vs poll + list RPC)
- Electron tray and custom window controls
- Full extension bridge feature parity with `extension-bridge.cjs`

## Serving wasm pitfalls

- **Address already in use**: prior `emrun` still bound; use existing URL or `PORT=8770`.
- **API calls fail**: wasm needs same-origin `/api` — standalone `emrun` only serves static files unless proxied.

## aqtinstall (dev bootstrap)

```bash
python3 -m venv .venv-aqt && .venv-aqt/bin/pip install aqtinstall
.venv-aqt/bin/aqt install-qt linux desktop 6.4.2 wasm_32 -O .qt
.venv-aqt/bin/aqt install-qt linux desktop 6.4.2 gcc_64 -O .qt
```

Qt wasm install warns that host `gcc_64` is required for cross-compilation.
