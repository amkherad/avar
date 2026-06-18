# 🧩 Layout

The Avar GUI uses a flexible layout you can resize and toggle.

## ↔️ Resizable panels

| Panel | Handle |
|-------|--------|
| Sidebar | Right edge of the sidebar |
| Detail panel | Between downloads and the right panel |
| Console | Top edge of the console drawer |

## 📌 Detail panel

The detail panel is **hidden by default**. Select a download in the list to open it. Toggle visibility with the **Details** button in the footer or **Ctrl+D**.

Modes (thumbtack icon in the panel header):

- **Pinned** — fixed column on the right
- **Inline** — appears below the download list

## 📊 Footer

The footer shows daemon health (uptime, active downloads, queue count) and optional **system monitors** (disk, memory, CPU, network) when enabled in **Settings → General**.

Toggle the detail panel and console from footer buttons. When **Histogram + values** is enabled for monitors, memory/CPU/network labels stay on one line with values overlaid on the chart at low opacity.
