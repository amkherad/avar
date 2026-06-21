# 🔌 Sessions

A **session** defines how the GUI connects to an Avar daemon.

## ✅ Active session

Use the session selector at the bottom of the sidebar to pick the active connection. All API calls (queues, downloads, health) use this session.

## 📝 Session fields

| Field | Description |
|-------|-------------|
| **Label** | Display name in the sidebar |
| **Server URL** | Base URL of the daemon HTTP API (e.g. `http://127.0.0.1:8000`) |
| **Auth token** | Optional bearer token when the daemon HTTP channel requires authentication (stored separately from other settings) |
| **Use dev proxy** | Route `/api` through the Vite dev server (for `npm run dev` only) |
| **Treat as remote session** | Override auto-detection when the daemon is remote but reached via a local address (for example an SSH tunnel to `127.0.0.1`) |

## 🌐 Local vs remote sessions

The GUI treats a session as **local** when its server URL uses a local address (`localhost`, `127.0.0.1`, `::1`, or other loopback forms). Built-in **Electron** and **dev proxy** sessions are always local.

Mark **Treat as remote session** when you connect through localhost to a daemon that runs on another machine. Remote sessions unlock **Copy to local folder** for completed downloads (see **Settings → General**).

## ➕ Adding, editing, and removing

- Click **Add session** or use the edit (pen) icon on an existing entry.
- Use **Remove** (trash icon) to delete a session after confirmation.
- Built-in sessions (such as the Electron tunnel) cannot be removed.

Use **Test connection** to verify reachability before saving.

## 🖥️ Electron desktop

The desktop app includes a default **Electron** session that tunnels API requests through the Electron main process to the local daemon. This is always available and is selected by default.

## 🔄 Connection status

The status indicator shows **Connected**, **Connecting**, or **Disconnected**. When disconnected, the GUI may show stale data with a warning banner until the connection is restored.
