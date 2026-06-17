# Layout and panels

The main dashboard uses a flexible layout you can adjust.

## Sidebar

The left sidebar is always visible and adapts to the current page:

| Page | Sidebar content |
|------|-----------------|
| **Dashboard** | Queue list (select a queue to filter downloads) |
| **Help** | Help topic list |
| **Settings** | Settings categories |
| **Manage queues** | Queue list for quick selection |

The **session selector** stays pinned at the bottom of the sidebar on every page.

- Drag the vertical handle between the sidebar and main content to resize (180–480 px).
- Use **Manage** in the queue panel to open the full queue management page (create, start, stop, delete).

## Downloads workspace

- **Toolbar** — add button and batch actions on the left; search on the right.
- **List** — scrollable download cards or table. Truncated text shows the full value on hover.
- **Detail panel** — optional right column for the primary selection.

## Footer

Shows daemon uptime, active download count, and queue count when connected.

Toggle buttons:

- **Console** — show or hide the log drawer. Turns red when new errors arrive while the console is closed; opening the console clears the indicator.
- **Details** — show or hide the download detail panel.

## Detail panel modes

| Mode | Behavior |
|------|----------|
| **Pinned** | Fixed column on the right; drag the handle left to widen, right to narrow (220–560 px) |
| **Inline** | Renders below the download list inside the card |

Switch modes with the thumbtack button in the panel header.

## Navigation

- Non-dashboard pages show a **back** arrow in the header to return to the main view.
- **Help** and **Settings** icons in the header toggle those pages.

## Popups

Double-click behavior opens a **download detail popup** in a separate Electron window (or browser tab) for focused inspection. Popup windows share the same daemon connection.

## Stale data banner

When the daemon is unreachable but cached data exists, a yellow banner warns that displayed information may be outdated. Connection failures are logged as errors in the console.
