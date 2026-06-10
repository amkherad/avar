---
name: queue-management
description: Design and implement download queue management for the Avar download manager. Apply when adding queue features, scheduler integration, CLI queue commands, or download-to-queue assignment.
---

# Queue Management

Apply this skill whenever working on download queues, queue CLI commands, scheduler queue integration, or assigning downloads to queues.

## Data Model

Queues are persisted in config at `dm.queues` as a JSON array. Each queue object has:

| Field | JSON key | Type | Notes |
|-------|----------|------|-------|
| Id | `id` | string | Unique; prefix `queue-` |
| Name | `name` | string | Unique human label |
| Description | `description` | string or null | Optional |
| Max concurrent downloads | `maxConcurrentDownloads` | number or null | Queue-level override |
| Max connections | `maxConnections` | number or null | Queue-level override |
| Temp path override | `tempPath` | string or null | Overrides `dm.tempPath` |
| Final save path override | `downloadPath` | string or null | Overrides `dm.downloadPath` |
| Started | `started` | boolean | Scheduler state; default `false` |

Download items in `dm.items` reference a queue via `queueId` (the queue **id**, not the name). When adding downloads, resolve queue names to ids through `queue_resolve_id()`.

Constants live in `avar.h` (`AVAR_CFG_DM_QUEUES`, `AVAR_QUEUE_FIELD_*`, `AVAR_FIELD_QUEUE_ID`).

## Module Boundaries

| Module | Responsibility |
|--------|----------------|
| `queue.c` / `queue.h` | Queue CRUD, detach/purge items, start/stop stubs |
| `queue_cli.c` | `avar queue` subcommands |
| `config.c` | Persistence (`dm.queues`, `dm.items`) |
| Scheduler (future) | Honor `maxConcurrentDownloads`, `maxConnections`, `started` |

Do not embed queue persistence logic in CLI or download modules. Call `queue_*` APIs.

## CLI Contract

```
avar queue add <name> [--description=<text>] [--maxConcurrentDownloads=<n>]
                    [--maxConnections=<n>] [--tempPath=<path>]
                    [--downloadPath=<path>]
avar queue rm <id> [--name=<name>] [--purge-items|--remove-items]
avar queue edit <id> [--description=<text>] ...
avar queue start <id> [--name=<name>]
avar queue stop <id> [--name=<name>]
avar queue ls
```

Rules:

- Options use `--key=value` form (camelCase keys match JSON fields).
- `rm` detaches items (`queueId` → null) by default.
- `--purge-items` / `--remove-items` remove matching `dm.items` entries.
- `start` logs and sets `started: true` until scheduler integration.
- `stop` sets downloading items back to `queued` and `started: false`.

## Future Iterations

When extending queues:

1. Add new fields to the queue JSON schema and `QueueOptions` / `QueuePatch`.
2. Expose new fields as `--key=value` CLI options on `add` and `edit`.
3. Add constants to `avar.h`; do not hardcode field names in multiple places.
4. Scheduler must read queue overrides (`tempPath`, `downloadPath`, limits) before global `dm.*` defaults.
5. Preserve backward compatibility: missing fields mean “use global default”.
6. Never store queue **names** in `dm.items.queueId`; always store queue **ids**.

## Review Checklist

- Queue logic in `queue.c`, not scattered across CLI/download
- Items reference queue by id
- Config changes are atomic (persist after mutation)
- Detach vs purge behavior preserved on remove
- New settings documented in this skill and `avar.h`
