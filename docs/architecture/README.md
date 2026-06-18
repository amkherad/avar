---
title: Architecture
sort: 3
---

# Architecture

This section describes how the major components of Avar fit together — the daemon, download engine, queues, configuration, and how clients connect.

## In this section

| Page | Contents |
|------|----------|
| [Overview]({{ site.baseurl }}/architecture/overview.html) | High-level component diagram and data flow |
| [Daemon mode]({{ site.baseurl }}/architecture/daemon-mode.html) | Local vs remote sessions, transports, JSON-RPC |
| [Download engine]({{ site.baseurl }}/architecture/download-engine.html) | Segmentation, resume, HLS, proxy |
| [Queues]({{ site.baseurl }}/architecture/queues.html) | Queue model and scheduling |
| [Configuration file]({{ site.baseurl }}/architecture/config-file.html) | Full `config.json` schema |

## Design principles

Avar is built as a **single C executable** that can act as a CLI tool, a background daemon, or a daemon with an embedded web UI (`avar-gui`). All transfer logic lives in the C backend. The GUI and browser extensions are thin clients that talk to the daemon over HTTP JSON-RPC.

Key properties:

- **One binary** — no separate daemon package to install
- **Persistent state** — downloads and queues survive restarts via `config.json` and per-download `state.json`
- **Pluggable transport** — CLI can reach the daemon via named pipe, Unix socket, or HTTP
- **Cross-platform** — Windows, Linux, and macOS with platform-specific service integration

## Source layout

| Directory | Role |
|-----------|------|
| `src/cli/` | Command-line parsing and subcommands |
| `src/daemon/` | Daemon server, RPC, transport, session routing |
| `src/download.c` | Download engine and state machine |
| `src/queue.c` | Queue management |
| `src/config.c` | Configuration persistence |
| `src/include/` | Public C headers |
| `gui/` | React SPA and Electron desktop shell |
| `extensions/` | Chromium and Firefox browser extensions |

Third-party libraries (mbedtls, mongoose, cJSON, argtable3) are vendored under `src/third_party/`.
