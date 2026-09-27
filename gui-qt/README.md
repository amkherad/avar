# Avar Qt GUI (`gui-qt`)

Native Qt 6 replacement for the Electron desktop shell. Talks to the Avar **daemon only over HTTP** (`/api/rpc`, `/api/health`, `/api/ws`, …). No JavaScript runtime and no embedded web UI.

## Layout

| Path | Role |
|------|------|
| `src/api/` | JSON-RPC daemon client |
| `src/theme/` | Theme tokens aligned with `gui/src/theme/themes.ts`; `FaIcon` loads Font Awesome SVGs |
| `third_party/fontawesome-free/` | Font Awesome Free solid SVGs (subset; see README there) |
| `src/sync/` | WebSocket / poll sync with the daemon |
| `src/extension/` | Client for the C extension bridge (`extensions/daemon`) |
| `src/ui/` | Widgets and pages mirroring the React shell |

Browser extension HTTP bridge logic lives in **`extensions/daemon`** (C only) so it can later move into the main daemon without rewriting the GUI.

## Requirements

- Qt **6.4+** with modules: Core, Gui, Widgets, Network (WebSockets and **Svg** recommended for Font Awesome icons; poll fallback when WebSockets missing)
- CMake **3.16+** and Ninja (recommended)
- C++23 compiler

### Desktop (primary)

```bash
./scripts/build_desktop.sh --config Release
# binary: gui-qt/build/desktop/avar-gui-qt
```

Optional: build the extension bridge daemon and run it beside the GUI:

```bash
cmake -S extensions/daemon -B extensions/daemon/build -G Ninja
cmake --build extensions/daemon/build
./extensions/daemon/build/avar-extension-daemon &
```

### WebAssembly

Install the Qt for WebAssembly kit, then:

```bash
export QT_ROOT=~/Qt/6.x.x/wasm_singlethread   # example
./scripts/build_wasm.sh
# output: gui-qt/build/wasm/avar-gui-qt.html (+ .wasm)
```

Wasm builds use **relative** daemon URLs (`/api/...`) so the UI can be served behind the same origin as the daemon HTTP endpoint.

## Configuration

Settings are stored with `QSettings` (organization `Avar`, application `gui-qt`):

- `daemon/baseUrl` — default `http://127.0.0.1:8000` (desktop) or empty for relative API (wasm)
- `daemon/authToken` — optional bearer token
- `theme` — `light`, `light-bright`, `queen-mode`, `dark`, or `system`
- `locale` — `en` or `fa` (RTL supported)

## Tests

```bash
cmake -S gui-qt -B gui-qt/build/test -DAVAR_GUI_QT_BUILD_TESTS=ON
cmake --build gui-qt/build/test
ctest --test-dir gui-qt/build/test --output-on-failure
```

## Parity with Electron (`gui/`)

```bash
python3 gui-qt/scripts/parity_audit.py   # writes PARITY.md + PARITY.json
./gui-qt/scripts/parity_loop.sh 600      # re-audit every 10 minutes
```

Use Cursor **`/loop 10m`** with a prompt like: *run `parity_audit.py`, fix the top gap in `gui-qt` to match `gui/`, re-audit* (agent-driven fixes; the shell loop only runs audits).

## Relationship to `gui/`

The existing React + Electron tree under `gui/` is unchanged. Visual tokens and shell structure are ported from `gui/src/theme/` and `gui/src/components/layout/`. Feature parity is incremental; this directory is the long-term native UI.
