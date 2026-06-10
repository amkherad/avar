# Avar Download Manager

## Skills

Available skills:

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
- Apply code-style.
- Preserve API compatibility.
- Prefer C23 features.
- Keep headers in include/.
- Keep implementations in src/.

## Daemon mode

CLI and daemon share one executable. Use the **daemon-mode** skill for all daemon, IPC, and session-routing work:

- Config lives at `daemon` in `config.json` (`daemon.session` controls local vs remote).
- CLI commands delegate through `daemon_session.c`; transports live in `daemon_transport.c`.
- Do not run the transfer engine in the CLI when `daemon.session.mode` is `remote`.
- Deferred work belongs in `tasks/daemon-mode-deferred.md`.

Project layout:

- include/ -> public headers
- src/ -> implementation files
- scripts/ -> automation scripts
- third_party/ -> vendored dependencies


## Project Layout

- Public headers are stored in include/
- C source files are stored directly in src/
- Scripts belong in scripts/
- Third-party dependencies belong in third_party/

## Dependency Policy

- All third-party libraries must be placed under third_party/
- If a dependency has an official Git repository, add it as a Git submodule
- Update .gitmodules when adding dependencies
- Do not vendor source code manually unless no repository exists
- Do not place third-party code in src/ or include/

## Language

- Target C23
- Use CMake
- Follow the code-style skill

## Testing

- Use doctest for unit tests
- Run AddressSanitizer and UndefinedBehaviorSanitizer in CI
