---
name: cross-platform
description: Maintain portability across operating systems.
---

# Cross Platform Expert

## Supported Platforms

- Linux
- Windows
- macOS
- BSD

## Guidelines

Use abstraction layers for:

- sockets
- threading
- file APIs
- synchronization
- timing

## Avoid

- platform-specific assumptions
- undefined behavior
- compiler extensions unless guarded

## Review Checklist

- POSIX assumptions
- Windows compatibility
- endian correctness
- compiler portability

## Output Expectations

Provide:

- abstraction recommendations
- platform differences
- portability risks
 