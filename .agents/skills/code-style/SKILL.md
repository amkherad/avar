# Project Structure

This project uses a flat source layout.

```text
avar/
├── CMakeLists.txt
├── src/
│   ├── http.c
│   ├── download.c
│   ├── scheduler.c
│   └── ...
├── include/
│   ├── http.h
│   ├── download.h
│   ├── scheduler.h
│   └── ...
├── scripts/
│   ├── build.sh
│   ├── format.py
│   └── ...
├── third_party/
│   └── ...
```

Rules:

* All public project headers must reside in `include/`.
* All C implementation files must reside directly in `src/`.
* `scripts/` contains Python and shell scripts only.
* `third_party/` contains external dependencies and vendored code.
* Never modify third-party code unless explicitly requested.
* Do not place project source files inside `include/`.
* Do not place implementation code inside headers unless explicitly required.
* Do not create nested module directories unless requested.

# Header Organization

For every module:

```text
src/http.c
include/http.h
```

```text
src/download.c
include/download.h
```

```text
src/scheduler.c
include/scheduler.h
```

Module names should match between source and header files.

Good:

```text
src/http_client.c
include/http_client.h
```

Bad:

```text
src/http_client.c
include/network.h
```

# Include Rules

Use project-relative includes from the include directory.

Good:

```c
#include "http.h"
#include "download.h"
#include "scheduler.h"
```

Avoid:

```c
#include "../include/http.h"
#include "../../include/http.h"
```

Do not include source files.

Never write:

```c
#include "http.c"
```

# Public vs Private APIs

Headers in `include/` are considered public project interfaces.

Expose only what other modules need.

Prefer forward declarations when possible.

Good:

```c
typedef struct DownloadTask DownloadTask;
```

Avoid exposing internal structure layouts unless necessary.

# Module Design

Each module should own a single responsibility.

Examples:

```text
http.c
```

* HTTP protocol handling

```text
download.c
```

* Download lifecycle management

```text
scheduler.c
```

* Task scheduling

```text
file_writer.c
```

* File operations

Avoid large "god modules".

Bad:

```text
network_utils.c
common.c
helpers.c
misc.c
everything.c
```

unless a clear architectural justification exists.

# CMake Expectations

Assume:

* Top-level `CMakeLists.txt`
* All project sources discovered from `src/`
* Public include directory is `include/`

Preferred:

```cmake
target_include_directories(
    downloader
    PUBLIC
        include
)
```

When generating build changes:

* Add new `.c` files to the build.
* Add matching headers to `include/`.
* Never place generated files in the project root.
* Never place project code in `third_party/`.

# Third-Party Libraries

External code belongs in:

```text
third_party/
```

Rules:

* Treat third-party code as external.
* Prefer wrappers instead of modifying vendor code.
* Isolate vendor-specific APIs behind project interfaces.
* Keep project style separate from vendor style.

Example:

```text
src/tls.c
include/tls.h
```

wraps:

```text
third_party/openssl/
```

rather than exposing OpenSSL throughout the codebase.

# Scripts

All automation belongs in:

```text
scripts/
```

Examples:

```text
scripts/build.sh
scripts/test.sh
scripts/format.py
scripts/release.py
```

Do not place build scripts in `src/`.

Do not place Python code in project modules.

Keep implementation code in C and automation in scripts.

# Dependency Policy

Prefer implementing functionality in-house when the complexity is reasonable.

Acceptable third-party dependencies:

- TLS library
- Compression library
- Testing framework

Avoid adding dependencies for:

- String utilities
- Containers
- Logging
- Thread pools
- HTTP client functionality

The downloader's core networking, scheduling, storage, and protocol logic should be implemented in project code.