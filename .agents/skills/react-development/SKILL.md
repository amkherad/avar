---
name: react-development
description: Design and implement the Avar React GUI in gui/. Apply when adding web UI, Electron desktop features, daemon HTTP client code, i18n, theming, or front-end configuration — not when editing C sources.
---

# React GUI Development

Apply this skill for all work under `gui/`. Do **not** apply C skills (`code-style`, `c-api-design`, `c-testing`, etc.) to TypeScript/React files.

## Stack

| Layer | Choice |
|-------|--------|
| Framework | React 19 + TypeScript |
| Bundler | Vite |
| Desktop | Electron (`electron/main.cjs`) |
| i18n | i18next + react-i18next |
| Styling | CSS variables + component classes (`avar-*`) |
| Config | `localStorage` via `src/config/` |

Pure SPA: no server-side rendering, no Node APIs in the renderer.

## Daemon API

The GUI talks to the Avar daemon over HTTP:

| Endpoint | Purpose |
|----------|---------|
| `GET /api/stats` | System statistics (poll sync / manual checks; SSE and WebSocket push stats) |
| `GET /api/health` | Uptime, active downloads, queue count |
| `POST /api/rpc` | JSON-RPC 2.0 |

Client: `src/api/daemon.ts` (`DaemonClient`).

Common RPC methods:

- `ping`, `health`
- `cli.exec` with `argv: ["avar", "queue", "ls"]` — list queues
- `cli.exec` with `argv: ["avar", "config", "get", "dm.items", "--format=json"]` — all download items
- `download.add` with `{ url, queue?, attached: false }`

Auth: optional `Authorization: Bearer <token>` when the daemon HTTP channel has a token configured.

Dev proxy: Vite proxies `/api` → `http://127.0.0.1:8000`. Sessions can enable **Use dev proxy** (`useRelativeApi`) for same-origin requests during `npm run dev`.

## Project layout

```text
gui/
├── electron/main.cjs      # Electron main process
├── src/
│   ├── api/               # Daemon client + types
│   ├── components/        # UI components (always use components)
│   │   ├── ui/            # Primitives (Button, Input, Modal, …)
│   │   ├── layout/
│   │   ├── session/
│   │   ├── queue/
│   │   └── download/
│   ├── config/            # GUI-only config (not avar config.json)
│   ├── hooks/
│   ├── i18n/locales/
│   ├── pages/
│   └── theme/
├── package.json
└── vite.config.ts
```

## Rules

1. **Components only** — pages compose components; avoid large inline JSX in pages.
2. **Themeable** — use CSS variables from `ThemeProvider`; never hard-code colors in components.
3. **i18n** — user-visible strings go in `src/i18n/locales/*.json`; support RTL via `document.documentElement.dir`.
4. **GUI config is separate** — session URLs, theme, locale, refresh interval live in `GuiConfig`, not the daemon `config.json`.
5. **No C changes for UI** — if the daemon lacks an API, prefer `cli.exec` over modifying C unless explicitly requested.
6. **Electron safety** — renderer: no `nodeIntegration`; main process loads `dist/index.html` in production.

## Commands

```bash
cd gui
npm install
npm run dev              # Web dev server (port 56821)
npm run dev:desktop      # Vite + Electron
npm run build            # Static SPA → dist/
npm run build:desktop    # Electron packages (mac, win, linux)
```

## Review checklist

- New strings added to all locale files (at least `en` and `fa`)
- RTL layout uses logical properties (`margin-inline`, `border-inline-end`) where possible
- Daemon errors surfaced to the user
- No secrets committed; auth tokens stay in localStorage only
