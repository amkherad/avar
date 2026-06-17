# Avar GUI Overview

Welcome to **Avar**, a download manager with a web and desktop interface. The GUI connects to the Avar daemon over HTTP and lets you manage queues, downloads, and sessions from one place.

## Main areas

| Area | Location | Purpose |
|------|----------|---------|
| **Header** | Top bar | App branding, back navigation, theme, Help, and Settings |
| **Sidebar** | Left (all pages) | Queues, help topics, or settings categories |
| **Session** | Bottom of sidebar | Active daemon session selector |
| **Downloads** | Center | Search, add, select, and control downloads |
| **Detail panel** | Right (optional) | Selected download metadata and actions |
| **Footer** | Bottom | Server health stats, console and detail panel toggles |
| **Console** | Bottom drawer | GUI and daemon log output |

## Navigation

- Use the **back** arrow in the header to return to the dashboard from other pages.
- Click **Settings** (gear icon) to open application settings.
- Click **Help** (question icon) to open in-app documentation with Markdown tables and diagrams.
- Press **Ctrl+1** to return to the main dashboard from anywhere.
- Use **Manage** in the queue sidebar to open the queue management page.

## Getting started

1. Ensure the Avar daemon is running with HTTP enabled (default `http://127.0.0.1:8000`).
2. Select or add a **session** in the sidebar if you connect to a remote daemon.
3. Choose a **queue** and click **Add download** to start a new transfer.

See the other help topics for details on each feature.
