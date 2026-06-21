# 📋 Queues

Queues organize downloads and control scheduling.

## 📌 Sidebar (dashboard)

The left sidebar lists queues in a **compact** view — only the queue name is shown. Hover a queue to see its description, running/stopped status, and download count.

Click a queue to filter the download list. Use **Manage** to open queue management in **Settings**.

## ⚙️ Settings → Queues

The queues settings page shows a full **table** with:

- **Search** — filter queues by name, description, status, or download count
- **Column sorting** — click column headers to sort; click again to reverse; a third click clears sorting
- **Column resize** — drag the edge of a column header to change width
- **Status filter** — use the dropdown in the Status column header
- **Multiselect** — enable checkboxes with the selection toggle, then select multiple queues for batch actions
- **Context menu** — right-click a queue row for Start, Stop, Modify, or Delete

### Batch actions

When one or more queues are selected, the toolbar shows **Start**, **Stop**, and **Delete** buttons. Batch delete asks for confirmation before removing all selected queues.

## ▶️ Queue actions

| Action | Description |
|--------|-------------|
| **Start** | Begin processing downloads in the queue |
| **Stop** | Pause the queue |
| **Add** | Create a new queue |
| **Modify** | Edit queue description |
| **Delete** | Remove a queue (downloads are detached) |

The **Default** queue is synthetic and shows downloads without an assigned queue. It cannot be started, stopped, modified, or deleted.
