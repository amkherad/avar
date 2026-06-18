---
title: Quick Start
sort: 3
parent: Getting Started
---

# Quick Start

This guide gets you from a fresh build to a completed download in a few minutes.

## 1. Start the daemon

The daemon runs downloads in the background and exposes an API for the CLI, GUI, and extensions.

```bash
avar daemon start --http --port=8000
```

Verify it is running:

```bash
avar daemon ping
# pong
```

To run in the foreground (useful for debugging):

```bash
avar daemon start --attached --http --port=8000
```

## 2. Download a file

The simplest form — pass a URL directly:

```bash
avar https://example.com/file.zip
```

This queues the download on the daemon. Add options as needed:

```bash
avar https://example.com/file.zip --queue=Default --name=my-file.zip
```

Equivalent explicit command:

```bash
avar dl https://example.com/file.zip --queue=Default
```

## 3. Manage queues

List queues:

```bash
avar queue ls
```

Create a queue with a concurrency limit:

```bash
avar queue add work --maxConcurrentDownloads=3 --downloadPath=~/Downloads/work
avar queue start work --name=work
```

## 4. Control downloads

```bash
avar dl pause dl-abc123
avar dl resume dl-abc123
avar dl stop dl-abc123
avar dl rm dl-abc123
```

Replace `dl-abc123` with the download ID shown when the download was created.

## 5. Watch progress

Attach a live terminal UI to the daemon:

```bash
avar daemon attach
```

Or stream logs:

```bash
avar daemon logs --follow
```

## 6. Open the GUI (optional)

With the daemon running on port 8000:

```bash
cd gui
npm install
npm run dev
```

Open `http://localhost:5173` in your browser. The dev server proxies API requests to the daemon.

For the desktop app:

```bash
npm run dev:desktop
```

## 7. Install as a service (optional)

Run Avar automatically on system startup:

```bash
# Linux (user service)
avar daemon install --systemd --user

# macOS
avar daemon install --launchd --user

# Windows (elevated shell)
avar daemon install --windows-service
```

## Next steps

- [CLI reference]({{ site.baseurl }}/cli/) — all commands and options
- [Configuration overview]({{ site.baseurl }}/getting-started/configuration-overview.html) — `config.json` and environment variables
- [GUI guide]({{ site.baseurl }}/gui/) — sessions, downloads, and settings
- [Browser extensions]({{ site.baseurl }}/extensions/) — capture media from web pages
