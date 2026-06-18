---
title: Getting Started
sort: 1
---

# Getting Started

This section walks you through installing Avar, starting the daemon, and running your first download.

## In this section

1. [Installation]({{ site.baseurl }}/getting-started/installation.html) — Build from source or download a release
2. [Quick start]({{ site.baseurl }}/getting-started/quick-start.html) — Your first download in a few commands
3. [Configuration overview]({{ site.baseurl }}/getting-started/configuration-overview.html) — Where settings live and how to change them

## What is Avar?

Avar is a download manager that runs as a single executable (`avar`). The same binary serves as:

- A **CLI** for scripting and quick downloads
- A **daemon** that keeps running in the background
- An **API server** that the GUI and browser extensions connect to

Downloads are organized into **queues**. Each queue can run independently with its own concurrency limits and output paths. The engine supports segmented parallel downloads, resume, HLS streams, and global or per-download proxy settings.

## Typical workflow

1. **Build or install** Avar (see [Installation]({{ site.baseurl }}/getting-started/installation.html))
2. **Start the daemon** with HTTP enabled so the GUI can connect:
   ```bash
   avar daemon start --http --port=8000
   ```
3. **Add a download** from the CLI, GUI, or browser extension
4. **Monitor progress** with `avar daemon attach`, the GUI, or `avar daemon logs --follow`

For day-to-day use you can install the daemon as a system service — see [Daemon commands]({{ site.baseurl }}/cli/daemon.html).
