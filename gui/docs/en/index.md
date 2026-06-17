# Avar GUI Overview

Welcome to **Avar**, a download manager with a web and desktop interface. The GUI connects to the Avar daemon over HTTP and lets you manage queues, downloads, and sessions from one place.

## Main areas

| Area | Location | Purpose |
|------|----------|---------|
| **Header** | Top bar | App branding, theme toggle, Help, and Settings |
| **Sidebar** | Left (main page) | Queue list and daemon session selector |
| **Downloads** | Center | Search, add, select, and control downloads |
| **Detail panel** | Right (optional) | Selected download metadata and actions |
| **Footer** | Bottom | Server health stats, console and detail panel toggles |
| **Console** | Bottom drawer | GUI and daemon log output |

## Navigation

- Click **Settings** (gear icon) in the header to open application settings. The main workspace is replaced entirely.
- Click **Help** (question icon) to open in-app documentation.
- Press **Ctrl+1** to return to the main dashboard from anywhere.

## Getting started

1. Ensure the Avar daemon is running with HTTP enabled (default `http://127.0.0.1:8000`).
2. Select or add a **session** in the sidebar if you connect to a remote daemon.
3. Choose a **queue** and click **Add download** to start a new transfer.

See the other help topics for details on each feature.
