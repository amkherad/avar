---
name: daemon-mode
description: Design and implement daemon mode, CLI-to-daemon communication, and service installation for the Avar download manager. Apply when adding daemon features, transport channels, session routing, or daemon CLI commands.
---

# Daemon Mode

Apply this skill whenever working on the background daemon, CLI delegation, transport channels, or `avar daemon` commands.

## Architecture

The CLI and daemon are the **same executable**. Operations route through two layers:

| Layer | Location | Responsibility |
|-------|----------|----------------|
| CLI commands | `src/cli/*_cli.c` | Parse args, call session or local APIs |
| Session router | `daemon/daemon_session.c` | Choose local execution vs remote delegation |
| Transport | `daemon/daemon_transport.c` | HTTP, named pipe, unix socket adapters |
| Daemon server | `daemon/daemon.c` | Lifecycle, PID file, channel startup |

```
CLI command ──► daemon_session ──► local module API
                      │
                      └── remote ──► transport ──► daemon server
```

Do not embed transport or RPC logic in individual CLI files. Use `daemon_session_*` and `daemon_transport_*`.

## Config Schema (`config.json`)

Root key: `daemon`.

```json
{
  "daemon": {
    "session": {
      "mode": "local",
      "transport": "pipe",
      "remoteHost": "127.0.0.1",
      "remotePort": 8000
    },
    "server": {
      "detach": true,
      "pidFile": "/path/to/avar.pid",
      "channels": {
        "http": { "enabled": true, "bind": "0.0.0.0", "port": 8000 },
        "pipe": { "enabled": true, "name": "\\\\.\\pipe\\avar" },
        "unix": { "enabled": false, "path": "/tmp/avar.sock" }
      }
    }
  }
}
```

### Session modes

| `daemon.session.mode` | Behavior |
|-----------------------|----------|
| `local` | CLI executes download/queue/config logic in-process |
| `remote` | CLI delegates to daemon via `daemon.session.transport` |

### Transports

| `daemon.session.transport` | Default use |
|----------------------------|-------------|
| `pipe` | CLI ↔ local daemon (default) |
| `unix` | CLI ↔ local daemon on Unix |
| `http` | Remote or local HTTP API |
| `local` | In-process only (no IPC) |

Constants live in `avar.h` (`AVAR_CFG_DAEMON_*`, `AVAR_DAEMON_TRANSPORT_*`).

Environment overrides use the `avar.` prefix plus the config key (e.g. `avar.daemon.session.mode=remote`).
When set, `get_config()` always returns the environment value over `config.json`.

## Module Boundaries

| Module | Responsibility |
|--------|----------------|
| `daemon/daemon_config.c` | Load defaults, merge CLI overrides |
| `daemon/daemon.c` | Server start/stop/restart, PID file |
| `daemon/daemon_transport.c` | Channel vtables (server + client ping) |
| `daemon/daemon_session.c` | CLI routing: local vs delegate |
| `daemon/daemon_install.c` | systemd unit generation (Linux) |
| `daemon/daemon_cli.c` | `avar daemon` subcommands |

## CLI Contract

```
avar daemon start [--attached] [--http] [--pipe] [--unix]
                  [--port=<n>] [--pipeName=<name>] [--unixPath=<path>]
                  [--pidFile=<path>] [--no-detach]
avar daemon stop [--wait] [--kill]
avar daemon restart
avar daemon attach
avar daemon status
avar daemon ping
avar daemon install [--systemd] [--user]
avar daemon uninstall [--systemd] [--user]
```

Common extensions (add when needed):

- `avar daemon reload` — reload config without full restart
- `avar daemon logs [--follow]` — stream daemon logs
- `avar daemon install --launchd` — macOS LaunchAgent
- `avar daemon install --windows-service` — Windows service

## Rules

1. **Same binary**: never split CLI and daemon into separate executables without explicit approval.
2. **Config-driven**: channel enablement, ports, paths, and session mode come from `config.json`; CLI flags override for `daemon start` only.
3. **Default CLI transport**: named pipe (`pipe` on Windows, configurable on Unix).
4. **Attached downloads**: when `daemon.session.mode` is `remote`, download/queue/config commands must delegate; do not run the transfer engine in the CLI process.
5. **Low-level reuse**: HTTP handlers, pipe framing, and unix listeners share request dispatch; do not duplicate business logic per channel.
6. **PID file**: `daemon.server.pidFile` identifies the running instance for stop/status/ping.
7. **Foreground first**: detached/forked startup may be deferred; document gaps in `tasks/`.

## Review Checklist

- Session routing goes through `daemon/daemon_session.c`
- Transport code stays in `daemon/daemon_transport.c`
- Config keys defined once in `avar.h`
- CLI help matches implemented subcommands
- Remote mode does not silently fall back to local execution
- Platform install paths documented (systemd user vs system unit)

## Deferred Work

See `tasks/daemon-mode-deferred.md` for completed iteration history and any new backlog items.
