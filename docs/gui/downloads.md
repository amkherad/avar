---
title: Downloads
sort: 3
parent: GUI
---

# Downloads

The downloads panel shows all items in the currently selected queue.

## Adding a download

Click **Add download** next to the search field (or press **Ctrl+N**). Enter a URL and choose:

- **Queue download** — adds the item to the active queue without starting it immediately.
- **Start now** — adds the item and begins downloading right away.

The dialog closes after either action succeeds. You can set a per-download proxy override in the add dialog.

## Search and filters

The search box filters by filename, URL, status, and other fields. Press **Ctrl+F** to focus search.

In **card view**, use the **Status filter** dropdown beside the search box.

In **table view**, use the status dropdown in the **Status** column header. Click **Status** or **Progress** column titles to sort.

## View modes

- **Card view** — stacked cards with progress bars.
- **Table view** — compact columns; drag column borders to resize.

## Selection

| Action | How |
|--------|-----|
| Select one | Click a row |
| Add to selection | **Ctrl+click** (or **Cmd+click** on macOS) |
| Range select | **Shift+click** from the last selected item |
| Checkbox mode | Toggle the checkbox button in the toolbar |
| Select all (table) | Header checkbox when checkbox mode is enabled |

When one or more downloads are selected, batch action buttons appear: pause/resume, start/stop, and delete.

**Double-click** a download to open the detail popup window.

## Deleting downloads

A confirmation dialog asks whether to remove the item from the list only, or also delete downloaded files from disk.

## Per-download actions

Actions are available from:

- The toolbar (for the current selection)
- The right **detail panel** (including **Copy curl**)
- The download detail popup window

## Detail panel

Toggle the right panel from the footer. Use the thumbtack control to switch between **pinned** (side column) and **inline** (below the list) layouts.

**Copy curl** generates an equivalent `curl` command. Small copy buttons beside filename, URL, and ID copy those values individually.

The detail panel also shows retry counts, error messages, and whether the filename was set manually or inferred automatically.

## Paging (table view)

When there are more rows than the page size (default **100**), paging controls appear below the table. Choose **20**, **50**, **100**, **500**, or **1000** rows per page.

## Keyboard shortcuts

| Action | Default |
|--------|---------|
| Add download | Ctrl+N |
| Focus search | Ctrl+F |
| Pause / resume | Ctrl+P |
| Start | Ctrl+Shift+S |
| Stop | Ctrl+Shift+X |
| Delete | Delete |

Remap shortcuts in **Settings → Shortcuts**. See [Shortcuts]({{ site.baseurl }}/gui/shortcuts.html).
