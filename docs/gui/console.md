---
title: Console
sort: 7
parent: GUI
---

# Console

The console is a bottom drawer for log output from the GUI and the connected daemon.

## Opening the console

Click **Console** in the footer or press **Ctrl+`**.

## Log sources

| Source | Description |
|--------|-------------|
| **GUI logs** | Messages from the web/desktop interface |
| **Daemon logs** | Log entries fetched from the daemon API |

Filter by minimum severity for each source.

## Options

- **Auto-scroll** — follow new entries automatically
- **Clear** — remove visible log lines

Resize the console using the handle above the drawer.

## CLI alternative

Stream daemon logs from the terminal:

```bash
avar daemon logs --follow
```

See [daemon command]({{ site.baseurl }}/cli/daemon.html).
