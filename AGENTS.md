# Avar Download Manager

## Skills

Skills are scoped by directory. Agents must pick the right set for the files being edited.

### C backend (`src/`, `include/`, `tests/`)

- code-style
- c-api-design
- c-testing
- performance
- dependency-management
- download-manager-architecture
- queue-management
- daemon-mode
- http-client
- network-programming
- memory-safety
- filesystem
- security

When modifying C code:

- Apply **code-style** (C only — not for `gui/`).
- Preserve API compatibility.
- Prefer C23 features.
- Keep headers in `include/`.
- Keep implementations in `src/`.

### React GUI (`gui/`)

- react-development
- gui-code-style

When modifying the web/desktop GUI:

- Apply **gui-code-style** and **react-development**.
- Do **not** apply C skills to TypeScript, CSS, or `gui/package.json`.
- Pure SPA: no server-side code in the GUI.
- Daemon access is HTTP JSON-RPC only (`src/api/daemon.ts`).

## Daemon mode

CLI and daemon share one executable. Use the **daemon-mode** skill for all daemon, IPC, and session-routing work:

- Config lives at `daemon` in `config.json` (`daemon.session` controls local vs remote).
- CLI commands delegate through `daemon/daemon_session.c`; transports live in `daemon/daemon_transport.c`.
- Do not run the transfer engine in the CLI when `daemon.session.mode` is `remote`.
- Deferred work belongs in `tasks/daemon-mode-deferred.md`.

Project layout:

- include/ -> public headers (`include/daemon/` for daemon APIs)
- src/ -> implementation files (`src/daemon/` for daemon module)
- gui/ -> React SPA + Electron desktop shell
- scripts/ -> automation scripts
- third_party/ -> vendored dependencies

## Project Layout

- Public headers are stored in include/
- C source files are stored directly in src/
- React GUI lives in gui/
- Scripts belong in scripts/
- Third-party dependencies belong in third_party/

## Dependency Policy

- All third-party libraries must be placed under third_party/
- GUI npm dependencies are managed in gui/package.json (not under third_party/)
- If a C dependency has an official Git repository, add it as a Git submodule
- Update .gitmodules when adding dependencies
- Do not vendor source code manually unless no repository exists
- Do not place third-party code in src/ or include/

## Language

- Backend: C23, CMake, code-style skill
- GUI: TypeScript, React, Vite, gui-code-style skill

## Testing

- C: doctest; AddressSanitizer and UndefinedBehaviorSanitizer in CI
- GUI: manual/E2E against a running daemon with HTTP enabled
