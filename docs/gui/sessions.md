---
title: Sessions
sort: 2
parent: GUI
---

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

## Adding, editing, and removing

- Click **Add session** or use the edit (pen) icon on an existing entry.
- Use **Remove** (trash icon) to delete a session after confirmation.
- Built-in sessions (such as the Electron tunnel) cannot be removed.

Use **Test connection** to verify reachability before saving.

## Electron desktop

The desktop app includes a default **Electron** session that tunnels API requests through the Electron main process to the local daemon. This is always available and is selected by default.

## Connection status

The status indicator shows **Connected**, **Connecting**, or **Disconnected**. When disconnected, the GUI may show stale data with a warning banner until the connection is restored.

## Remote daemons

To manage a daemon on another machine, add a session with that machine's URL:

```
http://192.168.1.100:8000
```

Ensure the daemon's HTTP channel is bound to an accessible interface (`daemon.server.channels.http.bind`) and that any firewall allows the port. Configure `authToken` if the remote daemon requires authentication.

## Dev proxy

When running `npm run dev`, enable **Use dev proxy** in the session settings. This routes API requests through Vite's `/api` proxy to `http://127.0.0.1:8000`, avoiding CORS issues during development.
