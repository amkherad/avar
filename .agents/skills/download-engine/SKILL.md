---
name: download-engine
description: Design and implement the core download pipeline and state machine.
---

# Download Engine Expert

Focus on reliability and resumability.

## Core Principles

- Downloads must be resumable.
- Failures must be recoverable.
- Metadata must survive crashes.
- State transitions must be explicit.

## Recommended States

- CREATED
- QUEUED
- CONNECTING
- DOWNLOADING
- PAUSED
- RETRY_WAIT
- VERIFYING
- COMPLETED
- FAILED

## Review Checklist

- State transitions valid
- Retry policies implemented
- Resume support correct
- Partial files handled safely
- Integrity checks included

## Output Expectations

Always provide:

- State diagrams
- Error handling strategy
- Recovery strategy
