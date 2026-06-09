---
name: c-testing
description: Design and implement unit, integration, stress, and sanitizer-based tests for a C23 download manager.
------------------------------------------------------------------------------------------------------------------

# C Testing Expert

Apply this skill whenever:

* Creating tests
* Reviewing test coverage
* Designing test infrastructure
* Investigating bugs

---

# Preferred Stack

Unit testing:

* doctest

Dynamic analysis:

* AddressSanitizer
* UndefinedBehaviorSanitizer

Optional:

* Valgrind

---

# Project Layout

```text
avar/
├── src/
├── include/
├── tests/
├── scripts/
├── third_party/
└── CMakeLists.txt
```

Tests belong in:

```text
tests/
```

---

# Test Naming

Source:

```text
src/http.c
```

Test:

```text
tests/test_http.c
```

Source:

```text
src/download.c
```

Test:

```text
tests/test_download.c
```

---

# Unit Tests

Unit tests must:

* run quickly
* avoid network access
* avoid filesystem dependencies
* be deterministic

Test:

* URL parsing
* HTTP parsing
* Range generation
* Retry logic
* Scheduler decisions

---

# Integration Tests

Integration tests verify:

* HTTP downloads
* redirects
* resume support
* chunked transfer
* TLS handling

Prefer local test infrastructure.

Example:

```text
scripts/http_test_server.py
```

Never depend on public internet services.

---

# Stress Tests

Test:

* large files
* thousands of queued downloads
* connection failures
* resume after interruption
* low disk space
* concurrent downloads

---

# Regression Tests

Every bug fix should include:

1. Reproduction test
2. Fix
3. Passing regression test

Never fix bugs without adding coverage.

---

# Assertions

Verify:

* return values
* state transitions
* resource cleanup
* error paths

Test failures should clearly identify root cause.

---

# Sanitizers

Development builds should enable:

```text
-fsanitize=address
-fsanitize=undefined
```

All tests must pass with sanitizers enabled.

Treat sanitizer failures as test failures.

---

# Coverage Expectations

Critical modules:

```text
http.c
download.c
scheduler.c
file_writer.c
```

Target:

* high branch coverage
* high error-path coverage

Coverage percentage alone is not sufficient.

---

# Mocking Strategy

Mock:

* filesystem
* sockets
* DNS
* TLS

Avoid mocking internal business logic.

---

# Network Testing

Verify:

* timeouts
* retries
* redirects
* partial downloads
* invalid responses

Use local servers whenever possible.

---

# File Testing

Verify:

* resume metadata
* partial files
* corruption recovery
* atomic rename
* sparse file support

---

# Performance Testing

Measure:

* throughput
* memory usage
* CPU utilization
* connection scaling

Use repeatable workloads.

Do not rely on subjective performance claims.

---

# Review Checklist

Verify:

* deterministic tests
* no internet dependency
* sanitizer coverage
* error-path coverage
* regression coverage
* integration coverage
* resource cleanup verification

---

# Output Requirements

When creating tests:

* Explain what is being tested.
* Explain failure conditions.
* Explain expected behavior.
* Identify missing coverage.
* Recommend sanitizer usage when relevant.
