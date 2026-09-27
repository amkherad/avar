# Extension bridge daemon (C)

HTTP bridge that mirrors `gui/electron/extension-bridge.cjs` and `gui/electron/extension-protocol.cjs`.
Kept separate from `src/` so it can be embedded in the main Avar daemon later without moving GUI code.

## Build

```bash
cmake -S extensions/daemon -B extensions/daemon/build -G Ninja
cmake --build extensions/daemon/build
./extensions/daemon/build/avar-extension-daemon --port 18766
```

## Endpoints (v1)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/v1/ping` | Health check |
| POST | `/v1` | Protocol envelope (`avar.extension` v1) |
| POST | `/extension/settings` | GUI → bridge configuration |
| GET | `/extension/status` | Legacy status |

The Qt GUI (`gui-qt`) may spawn this binary on desktop builds when it is present next to the executable or at `extensions/daemon/build/avar-extension-daemon` during development.
