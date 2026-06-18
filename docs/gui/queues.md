---
title: Queues
sort: 4
parent: GUI
---

# Queues

Queues organize downloads and control scheduling.

## Sidebar (dashboard)

The left sidebar lists queues in a **compact** view — only the queue name is shown. Hover a queue to see its description, running/stopped status, and download count.

Click a queue to filter the download list. Use **Manage** to open queue management in **Settings**.

## Queue actions

| Action | Description |
|--------|-------------|
| **Start** | Begin processing downloads in the queue |
| **Stop** | Pause the queue |
| **Add** | Create a new queue |
| **Modify** | Edit queue settings (from manage page) |
| **Delete** | Remove a queue (downloads are detached) |

The **Default** queue is synthetic and shows downloads without an assigned queue.

## Managing queues in Settings

Open **Settings → Queues** to create, edit, or delete queues. Available fields:

| Field | Description |
|-------|-------------|
| Name | Unique queue identifier |
| Description | Optional label |
| Max concurrent downloads | Simultaneous download limit |
| Max connections | Connections per download |
| Temp path | Temporary file directory |
| Download path | Output directory |

Changes are saved to the daemon's `config.json` via JSON-RPC.

## CLI alternative

All queue operations are also available from the terminal. See [queue command]({{ site.baseurl }}/cli/queue.html).
