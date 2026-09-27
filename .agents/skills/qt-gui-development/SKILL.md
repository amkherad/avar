---
name: qt-gui-development
description: >-
  Design and implement the native Qt 6 GUI in gui-qt/ and the C extension bridge
  in extensions/daemon/. Apply when editing gui-qt/, serving wasm builds, matching
  Electron/React parity, or extension HTTP bridge work — not when editing gui/
  TypeScript or src/ daemon C unless explicitly sharing assets or protocol constants.
---

# Qt GUI development (`gui-qt/`)

## Product goals

- **Replace the Electron shell** with a small native Qt app (desktop primary; WebAssembly secondary).
- **No JavaScript** in `gui-qt/` — C++23 and Qt Widgets/QSS only. Do **not** embed Chromium, QML with JS, or port the React app.
- Leave **`gui/`** (React + Electron) **unchanged** unless the user asks otherwise; treat it as the **visual and behavioral reference**.

## Isolation from the backend

| Rule | Detail |
|------|--------|
| Separate build | `gui-qt/` is its **own CMake project** — not added to the root Avar `CMakeLists.txt`. |
| No link to `src/` | Do not link the main daemon executable or backend objects into `avar-gui-qt`. |
| Daemon access | **HTTP only**: `/api/rpc`, `/api/health`, `/api/stats`, `/api/events`, `/api/ws` — mirror `gui/src/api/daemon.ts`. |
| Minimal `src/` touch | Avoid changing C daemon code; if HTTP behavior is wrong, fix the client or document a daemon gap. |

## Extension bridge (`extensions/daemon/`)

- **C only**, standalone CMake under `extensions/daemon/`.
- Implements the same protocol as `gui/electron/extension-protocol.cjs` / `gui/electron/extension-bridge.cjs` (port **18766**, `POST /v1`, `GET /v1/ping`, legacy `/extension/*`).
- Kept separate so it can move into the main daemon later **without** moving GUI code.
- **Desktop Qt** may spawn `avar-extension-daemon` when built and found beside the app or at `extensions/daemon/build/avar-extension-daemon` (see `ExtensionBridgeClient`).
- **Wasm**: no subprocess; extension integration is out-of-process on the host only.

When editing bridge behavior, update **`extensions/daemon/`** first, not Electron, unless deliberately syncing protocol docs.

## Hosting models

Compile-time flags from `cmake/AvarGuiQtOptions.cmake`:

| Mode | Macro | Daemon URL | Extension subprocess |
|------|--------|------------|----------------------|
| Desktop | `AVAR_GUI_HOSTING_DESKTOP` | `QSettings` default `http://127.0.0.1:8000` | Optional (`AVAR_GUI_QT_ENABLE_EXTENSION_SUBPROCESS`) |
| WebAssembly | `AVAR_GUI_HOSTING_WASM` | **Relative** `/api/*` (same origin as host) | Disabled |

`AppSettings::useRelativeDaemonApi()` is true on wasm. Serve the `.html/.wasm` behind the same origin as the daemon HTTP API (or proxy `/api`).

## Visual parity with Electron/React

- **Tokens**: Keep colors/radius/fonts aligned with `gui/src/theme/themes.ts` in `gui-qt/src/theme/ThemeTokens.cpp`.
- **Styles**: Port `avar-*` layout to `resources/styles/app.qss` using placeholders (`@@BG@@`, `@@PRIMARY@@`, …) filled by `StylesheetBuilder`.
- **Shell**: Match `gui/src/components/layout/` — `AppShell`, `Header`, sidebar context (queues / settings nav / help), stacked pages.
- **i18n**: Long-term, load `gui/src/i18n/locales/*.json`; until then extend `Translator` keys to match Electron copy (including emoji in settings category labels where used in `en.json`).
- **RTL**: Respect `fa` locale with `Qt::RightToLeft` like the web app.

Do not copy React component structure literally; reproduce **UX and styling** with Qt Widgets.

## Code layout (`gui-qt/`)

| Path | Role |
|------|------|
| `src/api/` | JSON-RPC `DaemonClient` — grow toward full `daemon.ts` surface |
| `src/sync/` | Health ping, poll fallback, optional `QWebSocket` when `Qt6WebSockets` is present |
| `src/theme/` | `ThemeManager`, QSS application |
| `src/ui/` | `MainWindow`, `AppShell`, pages, widgets |
| `src/config/` | `QSettings` (`Avar` / `gui-qt`) |
| `src/extension/` | HTTP client to `extensions/daemon` |
| `tests/` | Qt Test (`test_json_rpc`, `test_theme_tokens`, …) |
| `scripts/` | `build_desktop.sh`, `build_wasm.sh`, `serve_wasm.sh`, `parity_audit.py` |

## Build and run

**Desktop** (primary):

```bash
./gui-qt/scripts/build_desktop.sh --config Release
# → gui-qt/build/desktop/avar-gui-qt
```

**WebAssembly**:

- Requires **Emscripten** (Qt 6.4.x wasm expects ~**3.1.14**; repo may use `.emsdk` at repo root).
- Requires **Qt wasm** + **host** `gcc_64` kits (`QT_ROOT`, `QT_HOST_PATH`).

```bash
source .emsdk/emsdk_env.sh   # if using repo-local emsdk
export QT_ROOT=.qt/6.4.2/wasm_32
export QT_HOST_PATH=.qt/6.4.2/gcc_64
./gui-qt/scripts/build_wasm.sh
./gui-qt/scripts/serve_wasm.sh   # PORT=8765; detects already-serving
```

**Optional CMake packages**: `Qt6WebSockets` (live stream), `Qt6Svg` (header icon). If missing, use poll sync and text/icon fallback — do not hard-require them.

## Parity workflow

Before claiming feature parity with Electron:

1. Run `python3 gui-qt/scripts/parity_audit.py` → updates `gui-qt/PARITY.md` and `PARITY.json`.
2. Implement gaps in **priority order**: daemon API client → session/sync → dashboard downloads → settings categories → popups/tray.
3. Re-run audit and note score delta.

Audits compare pages, settings categories, `daemon.ts` methods, i18n coverage, theme tokens, and major layout features (footer, console, session selector, etc.).

## Implementation standards

- **C++23**, `QT_NO_CAST_*`, prefer `QStringLiteral`, testable seams (`IDaemonClient` where useful).
- **Crash handling**: `CrashHandler` — do not remove for desktop; wasm uses Qt message handler only.
- **Memory**: Prefer widgets + models over duplicate caches; one `QNetworkAccessManager` per client.
- **Async**: Daemon calls via QNetworkReply callbacks; UI updates on main thread with queued connections.
- **Tests**: Add Qt tests for pure logic (JSON-RPC, theme, parsing); no doctest in `gui-qt/`.

## Do not

- Add `.js`, `.ts`, or npm under `gui-qt/`.
- Link `gui-qt` into the all-in-one Electron/`build_all.py` pipeline without an explicit user request.
- Implement site-specific extension logic in Qt — extensions stay in `extensions/shared/` + `extensions/daemon/`.
- Use `QWebEngine` for the main UI (defeats size/memory goals).

## Related skills

- **gui-code-style** / **react-development** — reference only when porting UX from `gui/`.
- **extension-development** — browser capture; bridge HTTP is `extensions/daemon/`.
- **react-development** — do **not** apply to `gui-qt/` C++ files.

See **reference.md** in this skill for paths, env vars, and parity checklist details.
