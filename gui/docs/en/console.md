# Console

The console is a log viewer at the bottom of the main dashboard.

## Opening the console

- Click **Console** in the footer.
- Press **Ctrl+`** (backtick).

Drag the top edge of the console to resize it.

## Log sources

- **GUI logs** — messages from the front-end (actions, errors, debug).
- **Daemon logs** — recent lines fetched from the daemon via RPC.

Filter each source independently and set a minimum severity level.

## Controls

| Control | Description |
|---------|-------------|
| **Clear** | Remove all entries from the view |
| **Auto-scroll** | Follow new entries as they arrive |
| **Close** | Hide the console panel |

Daemon logs are polled periodically while the console is open and daemon logging is enabled.
