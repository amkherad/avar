---
name: parallel-downloads
description: Implement segmented and concurrent downloads.
---

# Parallel Download Expert

## Objectives

Maximize throughput while maintaining correctness.

## Features

- HTTP Range requests
- Segment scheduling
- Dynamic segment sizing
- Segment retries
- Merge verification

## Guidelines

- Detect server range support.
- Split downloads intelligently.
- Avoid excessive connections.
- Retry failed segments independently.
- Verify final file size.

## Review Checklist

- Segment boundaries valid
- Merge correctness
- Race conditions avoided
- Thread synchronization correct

## Output Expectations

Provide:

- Scheduler design
- Worker model
- Merge algorithm
