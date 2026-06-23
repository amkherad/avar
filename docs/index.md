---
title: Avar
---

# Avar Documentation

**Avar** is a cross-platform download manager written in C. It combines a command-line interface, a background daemon, a web and desktop GUI, and browser extensions into one system for managing HTTP/HTTPS downloads.

## What you can do

| Component | Purpose |
|-----------|---------|
| **CLI** | Add and control downloads, manage queues, configure the daemon from the terminal |
| **Daemon** | Run downloads in the background and expose a JSON-RPC API over HTTP, pipe, or Unix socket |
| **GUI** | Visual interface for sessions, queues, downloads, and settings (browser or Electron desktop) |
| **Extensions** | Capture media links from web pages and send them to Avar |

## Documentation sections

1. **[Getting started]({{ site.baseurl }}/getting-started/)** — Install Avar, run your first download, and learn the basics
2. **[CLI reference]({{ site.baseurl }}/cli/)** — Full command-line documentation for `avar` and its subcommands
3. **[Architecture]({{ site.baseurl }}/architecture/)** — How the daemon, download engine, queues, and configuration fit together
4. **[GUI]({{ site.baseurl }}/gui/)** — Using the web and desktop interface
5. **[Browser extensions]({{ site.baseurl }}/extensions/)** — Installing and using the Avar Download Helper

## Quick start

```bash
git clone --recursive {{ site.repo_url }}.git
cmake -S . -B output/build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build output/build

./output/build/avar daemon start --http --port=8000
./output/build/avar https://example.com/file.zip
```

Pre-built binaries will be available from [{{ site.download_url }}]({{ site.download_url }}) once releases are published.

## Source code

The project lives at [{{ site.repo_url }}]({{ site.repo_url }}). Report issues on [{{ site.issues_url }}]({{ site.issues_url }}).
