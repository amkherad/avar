# ⬇️ Downloads

The downloads panel shows all items in the currently selected queue.

## ➕ Adding a download

Click **Add download** in the downloads panel header (or press **Ctrl+N**) to open the add-download window. Enter a URL and choose:

- **Queue download** — adds the item to the active queue without starting it immediately.
- **Start now** — adds the item and begins downloading right away.

Use **Batch add** (beside **Add download** in the downloads panel header) to paste multiple URLs (one per line), review them in the batch dialog, then queue or start them together.

In the batch review window you can filter the list, use **Shift+click** for range selection, **Ctrl+click** (or **Cmd+click** on macOS) to add or remove individual rows, invert the selection, or remove unselected items before queuing.

The dialog closes after either action succeeds.

## 🔍 Search and filters

When any download is actively transferring, a **Stop all** button appears at the left end of the toolbar (stop icon). It stops every downloading item across all queues.

The search box is on the right side of the toolbar (no label — placeholder text only). It filters by filename, URL, status, and other fields. Press **Ctrl+F** to focus search.

In **card view**, use the **Status filter** dropdown beside the search box.

In **table view**, use the status dropdown in the **Status** column header. Click **Status** or **Progress** column titles to sort (click again to reverse, third click clears sorting).

## 👁️ View modes

- **Card view** — stacked cards with progress bars.
- **Table view** — compact columns; drag column borders to resize.

## ☑️ Selection

| Action | How |
|--------|-----|
| Select one | Click a row |
| Add to selection | **Ctrl+click** (or **Cmd+click** on macOS) |
| Range select | **Shift+click** from the last selected item |
| Checkbox mode | Toggle the checkbox button in the toolbar |
| Select all (table) | Header checkbox when checkbox mode is enabled |

When one or more downloads are selected, batch action buttons appear on the left side of the toolbar: pause/resume, start/stop, and delete.

**Double-click** a download to open the detail popup window. A single click only selects the row.

## 🗑️ Deleting downloads

When you delete, a confirmation dialog asks whether to remove the item from the list only, or also delete downloaded files from disk (checkbox).

## 🛠️ Per-download actions

The same actions are available from:

- The toolbar (for the current selection)
- The right **detail panel** (including **Copy curl**)
- The download detail popup window
- The row **context menu**

For **completed** downloads you can **Redownload** — the existing file is removed and the download is queued again with the same URL.

If a server does not support resuming a partial download, Avar stops the item and asks whether to **Restart download** (clear progress and reuse the same entry) or **New download** (keep the partial entry and queue a separate fresh download).

When connected to a **remote daemon**, **Copy to local folder** fetches the completed file from the remote server and saves it to the **Local download folder** configured under **Settings → General**. The remote daemon must have **`daemon.server.fileDownload.enabled`** set to `true` in `config.json` (off by default); enable it under **Settings → Daemon** when connected.

For **completed** downloads, the detail panel and popup include a **Tools** section with **checksum validation**. Choose an algorithm, optionally paste an expected hash, and click **Compute checksum**. The daemon reads the file from disk and returns the digest; when an expected value is provided, the UI shows whether it matches.

## ⌨️ Keyboard shortcuts

| Action | Default |
|--------|---------|
| Add download | Ctrl+N |
| Focus search | Ctrl+F |
| Pause / resume | Ctrl+P |
| Start | Ctrl+Shift+S |
| Stop | Ctrl+Shift+X |
| Delete | Delete |

Remap shortcuts in **Settings → Shortcuts**.

## 📄 Paging (table view)

When there are more rows than the page size (default **100**), paging controls appear below the table. Choose **20**, **50**, **100**, **500**, or **1000** rows per page.

## 📌 Detail panel

Toggle the right panel from the footer. Use the thumbtack control in the panel header to switch between **pinned** (side column) and **inline** (below the list) layouts.

Use **Copy curl** in the detail panel to generate an equivalent `curl` command and copy it to the clipboard.

Small copy buttons beside **filename**, **URL**, and **ID** copy those values individually.

The detail panel also shows **Retries** (errors vs. max retry limit), an **Error** message when a download fails, and whether the filename was **set by you** or **inferred automatically** from the download.
