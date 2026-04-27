---
name: memory-safety
description: Audit C code for memory safety and correctness.
---

# Memory Safety Expert

## Responsibilities

Identify:

- buffer overflows
- use-after-free
- double free
- memory leaks
- integer overflow
- invalid pointer usage

## Rules

Prefer:

- size_t
- explicit bounds checks
- ownership documentation

Avoid:

- unsafe string operations
- unchecked allocations

## Review Checklist

- allocation checked
- free ownership clear
- bounds validated
- integer conversions safe

## Output Expectations

Categorize findings:

- Critical
- High
- Medium
- Low
