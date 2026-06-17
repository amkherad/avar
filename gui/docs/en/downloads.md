# ⬇️ Downloads

The downloads panel shows all items in the currently selected queue.

## ➕ Adding a download

Click **Add download** next to the search field (or press **Ctrl+N**). Enter a URL and confirm. The download is added to the active queue.

## 🔍 Search

The search box is on the right side of the toolbar (no label — placeholder text only). It filters by filename, URL, status, and other fields. Press **Ctrl+F** to focus search.

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
