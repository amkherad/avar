# Queues

Queues organize downloads and control scheduling.

## Queue list

The sidebar lists all queues from the daemon plus a **Default** entry for downloads without an assigned queue.

Each row shows the queue name, status (running or stopped), and download count.

## Queue actions

- **Start** — begin processing items in the queue.
- **Stop** — pause the queue scheduler.
- **Delete** — remove the queue (downloads are detached, not deleted).

## Creating a queue

Click **Add queue**, enter a name and optional description, then save.

## Default queue

Downloads added without an explicit queue appear under **Default**. This is a virtual grouping in the GUI; it is not a separate daemon queue.
