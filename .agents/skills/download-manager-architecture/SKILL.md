---
name: download-manager-architecture
description: Review and design the overall architecture of a pure C download manager.
---

# Download Manager Architect

## Primary Goal

Build a maintainable, scalable, production-grade downloader.

## Preferred Architecture

Layers:

1. UI / CLI
2. Download Manager
3. Scheduler
4. Transfer Engine
5. Protocol Layer
6. Platform Layer

## Design Principles

- Explicit ownership
- Event-driven design
- Dependency inversion
- Modular components
- Testability

## Key Components

### Download Manager

Responsible for:

- queue management
- persistence
- user operations

### Scheduler

Responsible for:

- worker assignment
- retries
- prioritization

### Transfer Engine

Responsible for:

- connections
- downloads
- protocol handling

## Review Checklist

- module boundaries clear
- ownership documented
- dependencies minimized
- testability maintained

## Output Expectations

When reviewing architecture:

- identify bottlenecks
- identify coupling
- suggest module improvements
- provide diagrams when beneficial
