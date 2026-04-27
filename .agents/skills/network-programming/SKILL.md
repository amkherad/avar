---
name: network-programming
description: Implement scalable network communication using low-level C APIs.
---

# Network Programming Expert

## Preferred Technologies

Linux:

- sockets
- epoll
- nonblocking I/O

Fallback:

- poll
- select

## Requirements

- No blocking operations in event loop
- Handle thousands of sockets
- Minimize syscalls
- Avoid busy waiting

## Review Checklist

- Proper socket lifecycle
- Nonblocking correctness
- Error handling
- Timeout implementation
- Resource cleanup

## Performance Priorities

1. epoll
2. edge-triggered operation
3. connection pooling
4. DNS caching

## Output Expectations

Explain:

- event flow
- socket ownership
- cleanup strategy
