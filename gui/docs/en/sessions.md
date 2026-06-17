# Sessions

A **session** defines how the GUI connects to an Avar daemon.

## Active session

Use the session selector at the bottom of the sidebar to pick the active connection. All API calls (queues, downloads, health) use this session.

## Session fields

| Field | Description |
|-------|-------------|
| **Label** | Display name in the sidebar |
| **Server URL** | Base URL of the daemon HTTP API (e.g. `http://127.0.0.1:8000`) |
| **Auth token** | Optional bearer token when the daemon HTTP channel requires authentication |
| **Use dev proxy** | Route `/api` through the Vite dev server (for `npm run dev` only) |

## Adding and editing

Click **Add session** or edit an existing entry. Use **Test connection** to verify reachability before saving.

## Connection status

The status indicator shows **Connected**, **Connecting**, or **Disconnected**. When disconnected, the GUI may show stale data with a warning banner until the connection is restored.
