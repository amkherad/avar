---
title: Queues
sort: 4
parent: Architecture
---

# Queues

Queues are the primary organizational unit in Avar. Each queue has its own concurrency limits, paths, and running state.

## Queue model

Queues are stored in the `dm.queues` array in `config.json`. Each entry contains:

| Field | Description |
|-------|-------------|
| `name` | Unique queue name |
| `description` | Optional description |
| `maxConcurrentDownloads` | Max simultaneous downloads |
| `maxConnections` | Max connections per download |
| `maxRetries` | Retry limit on failure |
| `tempPath` | Temporary file directory |
| `downloadPath` | Final output directory |
| `started` | Whether the queue is actively processing |

## Scheduling

A queue must be **started** before it processes downloads:

```bash
avar queue start my-queue --name=my-queue
```

When started, the queue scheduler picks queued downloads up to `maxConcurrentDownloads`. Stopping a queue prevents new downloads from starting; active ones may finish depending on configuration.

## Default queue

Downloads without an explicit queue assignment appear under a synthetic **Default** queue in the GUI. This is a view-level grouping, not a persisted queue entry.

## Queue lifecycle

```
create → (stopped) → start → (running) → stop → (stopped) → remove
```

Removing a queue detaches its downloads — they are not deleted. Downloads previously in the removed queue move to the Default grouping.

## CLI management

All queue operations are available from the CLI:

```bash
avar queue add work --maxConcurrentDownloads=3
avar queue start work --name=work
avar queue stop work --name=work
avar queue edit work --name=work --maxConcurrentDownloads=5
avar queue rm work --name=work
avar queue ls
```

See [queue command]({{ site.baseurl }}/cli/queue.html) for full syntax.

## GUI management

Queues appear in the sidebar on the dashboard. Use **Manage** to open the queue management page in Settings. See [Queues (GUI)]({{ site.baseurl }}/gui/queues.html).

## Multi-queue workflows

A common pattern is to separate downloads by type or priority:

| Queue | Purpose | Concurrency |
|-------|---------|-------------|
| `fast` | Small files, quick turnaround | 6 |
| `large` | Large archives, limited bandwidth | 2 |
| `media` | Video/audio HLS streams | 3 |

Create queues with appropriate `downloadPath` values to keep output organized on disk.
