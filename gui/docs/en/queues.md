# Queues

Queues organize downloads and control scheduling.

## Queue list (dashboard sidebar)

The sidebar lists all queues from the daemon plus a **Default** entry for downloads without an assigned queue.

Each row shows the queue name, status (running or stopped), and download count. Select a queue to filter the download list.

From the sidebar you can **Start** or **Stop** a queue. Use **Manage** to open the full queue management page.

## Manage queues page

The management page provides full queue control:

- **Add queue** — create a new queue with name and optional description.
- **Start / Stop** — control queue scheduling.
- **Delete** — remove the queue (downloads are detached, not deleted).

## Default queue

Downloads added without an explicit queue appear under **Default**. This is a virtual grouping in the GUI; it is not a separate daemon queue.
