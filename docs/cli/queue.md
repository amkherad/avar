---
title: queue
sort: 3
parent: CLI Reference
---

# queue

Queues organize downloads and control how many run at the same time.

```
avar queue <subcommand> [options]
```

## Subcommands

### add

```
avar queue add <name>
    [--description=<text>]
    [--maxConcurrentDownloads=<n>]
    [--maxConnections=<n>]
    [--tempPath=<path>]
    [--downloadPath=<path>]
```

Create a new queue. Prints the new queue ID on success.

| Option | Description |
|--------|-------------|
| `--description` | Human-readable description |
| `--maxConcurrentDownloads` | Maximum simultaneous downloads in this queue |
| `--maxConnections` | Maximum connections per download |
| `--tempPath` | Temporary file directory for this queue |
| `--downloadPath` | Final output directory for this queue |

### rm

```
avar queue rm <id> [--name=<name>] [--purge-items|--remove-items]
```

Remove a queue. Downloads in the queue are detached, not deleted.

### edit

```
avar queue edit <id>
    [--description=<text>]
    [--maxConcurrentDownloads=<n>]
    [--maxConnections=<n>]
    [--tempPath=<path>]
    [--downloadPath=<path>]
```

Update queue settings. Only specified options are changed.

### start

```
avar queue start <id> [--name=<name>]
```

Begin processing downloads in the queue.

### stop

```
avar queue stop <id> [--name=<name>]
```

Pause the queue. Active downloads may finish; new ones will not start.

### ls

```
avar queue ls
```

List all queues. Output is tab-separated: `id`, `name`, optional description, and `(running)` if the queue is started.

## Resolving queues

Most subcommands accept either a queue **ID** (`queue-…`) or a **name** via `--name`:

```bash
avar queue start work --name=work
avar queue stop queue-abc123
```

## Examples

```bash
# Create a queue for large files
avar queue add large-files \
  --description="Slow, large downloads" \
  --maxConcurrentDownloads=2 \
  --downloadPath=~/Downloads/large

# Start it
avar queue start large-files --name=large-files

# List queues
avar queue ls

# Edit concurrency
avar queue edit large-files --name=large-files --maxConcurrentDownloads=4

# Remove queue (downloads are detached)
avar queue rm large-files --name=large-files
```

## Default queue

A synthetic **Default** queue exists for downloads without an assigned queue. It appears in the GUI sidebar but is not a persisted queue entry.
