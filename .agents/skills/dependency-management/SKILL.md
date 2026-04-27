---
name: dependency-management
description: Manage third-party dependencies using Git submodules under third_party and maintain consistent repository structure.
---------------------------------------------------------------------------------------------------------------------------------

# Dependency Management Expert

Apply this skill whenever:

* Adding a dependency
* Updating a dependency
* Replacing a dependency
* Modifying build configuration
* Introducing external libraries

---

# Project Dependency Policy

All third-party dependencies must reside in:

```text
third_party/
```

Examples:

```text
third_party/openssl/
third_party/zlib/
third_party/criterion/
```

Never place third-party code in:

```text
src/
include/
scripts/
```

---

# Preferred Dependency Acquisition Method

When a dependency is available as a Git repository:

Use a Git submodule.

Example:

```bash
git submodule add https://github.com/madler/zlib.git third_party/zlib
```

Do not:

* Copy source files into the repository (when possible).
* Download release archives and commit them.
* Vendor code manually.

Preferred order:

1. Git submodule
2. System package (if explicitly requested)
3. Manual vendoring (only if no repository exists)

---

# Git Submodule Rules

When adding a dependency:

1. Create a submodule inside `third_party/`.
2. Update `.gitmodules`.
3. Pin to a specific commit.
4. Document the dependency.
5. Integrate through CMake.

Expected result:

```text
.gitmodules

third_party/
├── openssl/
├── zlib/
└── criterion/
```

---

# Repository Structure

Good:

```text
project/
├── src/
├── include/
├── scripts/
├── third_party/
│   ├── openssl/
│   ├── zlib/
│   └── criterion/
├── .gitmodules
└── CMakeLists.txt
```

Bad:

```text
project/
├── vendor/
├── external/
├── deps/
└── libraries/
```

Use only:

```text
third_party/
```

unless explicitly instructed otherwise.

---

# Dependency Evaluation

Before recommending a dependency:

Evaluate:

* Maintenance status
* License compatibility
* Build system support
* Platform compatibility
* Security history
* Repository activity

Prefer:

* Mature projects
* Active maintenance
* Permissive licenses
* Stable APIs

---

# Existing Dependencies

Before adding a dependency:

Check whether existing project dependencies already provide the capability.

Avoid introducing duplicate functionality.

Example:

If OpenSSL is already present:

Do not add another TLS library unless requested.

---

# Build Integration

Preferred CMake integration:

```cmake
add_subdirectory(
    third_party/zlib
)
```

or

```cmake
target_link_libraries(
    downloader
    PRIVATE
    zlib
)
```

Avoid global include pollution.

---

# Wrapping Third-Party Libraries

Prefer creating project-owned wrappers.

Example:

```text
include/tls.h
src/tls.c
```

wrapping:

```text
third_party/openssl/
```

Do not expose third-party APIs throughout the codebase unless necessary.

Benefits:

* Easier replacement
* Reduced coupling
* Cleaner public interfaces

---

# Updating Dependencies

When updating a dependency:

* Update submodule commit.
* Review release notes.
* Check API compatibility.
* Verify build success.
* Run test suite.
* Verify no new warnings.

Never update dependencies blindly.

---

# Security Updates

Security fixes should be prioritized.

When a dependency has a known vulnerability:

1. Identify affected versions.
2. Update submodule commit.
3. Verify compatibility.
4. Run full tests.
5. Document upgrade.

---

# Licensing

Before adding a dependency:

Verify:

* License type
* Redistribution requirements
* Attribution requirements

Highlight any license concerns.

---

# Download Manager Specific Guidance

Acceptable third-party categories:

* TLS
* Compression
* Testing frameworks
* DNS libraries (when justified)

Avoid dependencies for:

* Logging
* Containers
* String manipulation
* Scheduling
* Download management
* HTTP client functionality

Core downloader functionality should remain project-owned.

---

# Review Checklist

Verify:

* Dependency is necessary.
* Dependency lives in third_party/.
* Git submodule is used when available.
* .gitmodules updated.
* Specific commit pinned.
* License reviewed.
* Wrapper layer considered.
* CMake integration correct.
* No duplicate functionality introduced.

---

# Output Requirements

When recommending a dependency:

Provide:

* Git repository URL
* Reason for selection
* License summary
* Expected integration approach
* Suggested submodule path

When adding a dependency:

Assume Git submodule installation unless explicitly instructed otherwise.
