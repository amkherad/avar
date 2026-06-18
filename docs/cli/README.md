---
title: CLI Reference
sort: 2
---

# CLI Reference

The `avar` executable is both the command-line interface and the daemon. All download management can be done from the terminal.

## Global options

These flags work with any command:

| Flag | Description |
|------|-------------|
| `-h`, `--help` | Show help for the current command |
| `-v`, `--version` | Print version and exit |
| `--verbose` | Increase log verbosity |

Run `avar` with no arguments to see top-level help.

## Command overview

```
avar [global options]
├── <url>                    # Shorthand download (HTTP/HTTPS)
├── download | dl            # Download management
├── queue                    # Queue management
├── config                   # Configuration
├── profile                  # Profiles (planned)
├── scheduler                # Schedulers (planned)
└── daemon                   # Daemon control
```

## In this section

| Page | Contents |
|------|----------|
| [URL shorthand]({{ site.baseurl }}/cli/url-shorthand.html) | Download by passing a URL directly |
| [download]({{ site.baseurl }}/cli/download.html) | Add, pause, resume, stop, and remove downloads |
| [queue]({{ site.baseurl }}/cli/queue.html) | Create and manage download queues |
| [config]({{ site.baseurl }}/cli/config.html) | Read and write configuration values |
| [daemon]({{ site.baseurl }}/cli/daemon.html) | Start, stop, attach, logs, and service installation |

## Remote session mode

When `daemon.session.mode` is set to `remote`, most commands are forwarded to the running daemon instead of executing locally. This lets you control a remote or background daemon from any terminal.

Check the current mode:

```bash
avar config get daemon.session.mode
```

Switch to remote mode:

```bash
avar config set daemon.session.mode remote
```

## Download and queue IDs

- Download IDs are prefixed with `dl-` (e.g. `dl-a1b2c3`)
- Queue IDs are prefixed with `queue-`

Use `avar queue ls` to list queue IDs and names. Download IDs are printed when a download is created.

## Getting help

Every subcommand supports `--help`:

```bash
avar dl --help
avar queue add --help
avar daemon start --help
```
