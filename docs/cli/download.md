---
title: download
sort: 2
parent: CLI Reference
---

# download

Manage individual downloads. The command can be abbreviated as `dl`.

```
avar download <subcommand> [options]
avar dl <subcommand> [options]
```

## Download a URL

```
avar dl <url> [--attached] [--queue=<name>] [--proxy=<url>]
```

Same as the [URL shorthand]({{ site.baseurl }}/cli/url-shorthand.html). Queues a download on the daemon.

## Subcommands

### pause

```
avar dl pause <id>
```

Pause an active download. The partial file and resume state are preserved.

### resume

```
avar dl resume <id>
```

Resume a paused download.

### start

```
avar dl start <id>
```

Start a queued or stopped download.

### stop

```
avar dl stop <id>
```

Stop a download without removing it from the list.

### rm

```
avar dl rm <id|name> [--force] [--purge-files]
```

Remove a download from the list.

| Option | Description |
|--------|-------------|
| `--force` | Skip confirmation |
| `--purge-files` | Also delete downloaded files from disk |

### ls

```
avar dl ls [--list]
```

List all downloads. *(Listing output is being expanded — use the GUI or `avar daemon attach` for a live view in the meantime.)*

### add

```
avar dl add <name>
```

Add a download by name. *(Planned — not yet fully implemented.)*

## Examples

```bash
# Queue and start via URL
avar dl https://example.com/file.zip --queue=Default

# Pause, then resume
avar dl pause dl-abc123
avar dl resume dl-abc123

# Remove download and delete files
avar dl rm dl-abc123 --purge-files --force
```

## Download statuses

| Status | Meaning |
|--------|---------|
| `queued` | Waiting to start |
| `downloading` | Actively transferring |
| `paused` | Paused by user |
| `stopped` | Stopped but retained in the list |
| `completed` | Finished successfully |
| `failed` | Ended with an error |

## Remote mode

When `daemon.session.mode` is `remote`, the entire `download` command is delegated to the daemon. Ensure the daemon is running and reachable before issuing commands.
