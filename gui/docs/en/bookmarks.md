# Bookmarks

Save pages from the browser extension or add them manually in the GUI, then open them later when you are ready to capture downloads.

## Sidebar

On the dashboard, **Bookmarks** appears below your queue list in the left panel. Select it to view all saved pages for the connected daemon session.

The bookmark table shows:

- Page title
- URL
- Link count (how many links the extension counted on the page when you bookmarked it)

Odd and even rows use alternating background colors for easier scanning.

## Selection

- Click a row or its checkbox to select it.
- **Ctrl+click** (Cmd on macOS) toggles individual rows.
- **Shift+click** selects a range between the last anchor row and the clicked row.
- Use the header checkbox to select or clear all visible rows (respects the current search filter).

When one or more bookmarks are selected, the toolbar shows the selection count and a **Remove selected** button.

## Actions

| Action | Description |
|--------|-------------|
| **Add (+)** | Opens a dialog to save a page URL manually |
| **Open page** | Opens the bookmark URL in your default browser |
| **Copy link** | Copies the page URL to the clipboard |
| **Remove** | Deletes a single bookmark from the daemon |
| **Remove selected** | Deletes all selected bookmarks after confirmation |
| **Search** | Filters the table by title, URL, or link count |
| **Import** | Loads bookmarks from a JSON file into the daemon |
| **Export** | Saves the current bookmark list to a JSON file on your computer |

## Browser extension

In the extension popup:

- When **Put selected links in a separate tab** is enabled in extension settings, use the **Bookmark** tab.
- When that option is off, the bookmark panel appears at the bottom of the media list.

The panel shows the current page title, URL, link count, and a button to **Bookmark this page** or **Remove bookmark** if the page is already saved.

## Storage path

Bookmarks are stored in a JSON file on the daemon machine (default: `bookmarks.json` next to `config.json`). Change the path under **Settings → Daemon → Bookmarks storage**.
