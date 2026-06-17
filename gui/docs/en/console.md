# Console

The console is a log viewer at the bottom of the main dashboard.

## Opening the console

- Click **Console** in the footer.
- Press **Ctrl+`** (backtick).

When new **error**-level entries arrive while the console is closed, the **Console** button turns red. Opening the console clears the indicator.

Drag the top edge of the console to resize it.

## Log sources

- **GUI logs** — messages from the front-end (actions, errors, debug).
- **Daemon logs** — recent lines fetched from the daemon via RPC.

Filter each source independently and set a minimum severity level.

Failed daemon connections are logged as **errors** in the GUI log stream.

## Controls

| Control | Description |
|---------|-------------|
| **Clear** | Remove all entries from the view |
| **Auto-scroll** | Follow new entries as they arrive |
| **Close** | Hide the console panel |

Logs are retained while the console is closed so you can review them after opening it. Daemon logs are polled periodically while the console is open and daemon logging is enabled.
