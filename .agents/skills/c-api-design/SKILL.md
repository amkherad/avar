---
name: c-api-design
description: Design consistent, maintainable, and high-performance public APIs for a C23 download manager.
----------------------------------------------------------------------------------------------------------

# C API Design Expert

Apply this skill whenever:

* Creating a new module
* Designing public headers
* Refactoring interfaces
* Reviewing API consistency

---

# Goals

Prioritize:

1. Correctness
2. Clear ownership
3. Ease of use
4. Maintainability
5. Performance

Public APIs should be difficult to misuse.

---

# Project Layout

Public headers:

```text
include/
```

Implementation:

```text
src/
```

Each module consists of:

```text
include/http.h
src/http.c
```

```text
include/download.h
src/download.c
```

---

# Module Design

Each module should have a single responsibility.

Good:

```text
http
scheduler
download
tls
file_writer
```

Avoid:

```text
common
helpers
utils
misc
```

---

# Opaque Types

Prefer opaque structures.

Public header:

```c
typedef struct DownloadTask DownloadTask;
```

Implementation:

```c
struct DownloadTask
{
    ...
};
```

Hide implementation details whenever possible.

---

# Ownership Rules

Every API must clearly communicate ownership.

## Caller Owns

```c
char* url_duplicate(
    const char* url
);
```

Document:

```c
/**
 * Caller owns returned memory.
 */
```

## Callee Owns

```c
const char* download_name(
    const DownloadTask* task
);
```

Document:

```c
/**
 * Returned pointer remains valid
 * while task exists.
 */
```

Never leave ownership ambiguous.

---

# Constructors And Destructors

Prefer explicit lifecycle functions.

```c
DownloadTask* download_create(void);

void download_destroy(
    DownloadTask* task
);
```

Avoid hidden allocations.

---

# Initialization Pattern

For stack objects:

```c
bool scheduler_init(
    Scheduler* scheduler
);

void scheduler_deinit(
    Scheduler* scheduler
);
```

---

# Error Handling

Never use errno as a public API.

Prefer typed errors.

```c
typedef enum DownloadError
{
    DownloadErrorNone,
    DownloadErrorNetwork,
    DownloadErrorTimeout,
    DownloadErrorInvalidUrl
} DownloadError;
```

Return:

```c
DownloadError
```

or

```c
bool
```

with explicit error retrieval.

---

# Boolean APIs

Good:

```c
bool download_pause(
    DownloadTask* task
);
```

Bad:

```c
int pause_download(
    void* p
);
```

---

# Const Correctness

Use const aggressively.

```c
bool http_parse_url(
    const char* url,
    ParsedUrl* parsed
);
```

---

# Input Validation

Public APIs validate:

* NULL pointers
* lengths
* enum values
* ranges

Never trust callers.

---

# Buffer APIs

Prefer:

```c
DownloadError http_format_request(
    char* buffer,
    size_t buffer_size
);
```

Avoid:

```c
char* http_format_request(void);
```

for hot paths.

---

# Memory Allocation Policy

Avoid allocations in frequently called APIs.

Good:

```c
bool parser_parse(
    Parser* parser,
    const uint8_t* data,
    size_t length
);
```

Bad:

```c
ParsedResult* parser_parse(
    const uint8_t* data
);
```

---

# Thread Safety

Every public API should document:

* Thread-safe
* Single-threaded
* External synchronization required

Do not leave behavior unspecified.

---

# Versioning

Adding parameters:

Prefer:

```c
DownloadOptions
```

over long parameter lists.

```c
typedef struct DownloadOptions
{
    uint32_t timeout_ms;
    uint32_t retries;
} DownloadOptions;
```

---

# Header Hygiene

Headers should include only required dependencies.

Prefer forward declarations.

Avoid transitive include chains.

---

# Review Checklist

Verify:

* ownership documented
* opaque types used
* no hidden allocations
* const correctness
* explicit lifecycle
* typed errors
* minimal dependencies
* thread-safety documented

---

# Output Requirements

When designing APIs:

* Explain ownership.
* Explain lifetime.
* Explain thread-safety.
* Explain error handling.
* Explain performance implications.
