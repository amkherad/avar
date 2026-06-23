---
title: Installation
sort: 2
parent: Getting Started
---

# Installation

## Pre-built releases

When available, download the latest release for your platform from [{{ site.download_url }}]({{ site.download_url }}).

Extract the archive and place the `avar` binary on your `PATH`. On Windows, add the folder containing `avar.exe` to your system or user `PATH`.

## Build from source

### Prerequisites

| Requirement | Notes |
|-------------|-------|
| **CMake** | 4.1 or newer |
| **C compiler** | C23 support |
| **Git** | Clone with submodules (`--recursive`) |
| **Ninja** | Recommended generator (any CMake generator works) |
| **Node.js 20+** | Only needed for GUI builds |

### Clone the repository

```bash
git clone --recursive {{ site.repo_url }}.git
cd avar
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

### Build the CLI and daemon

```bash
cmake -S . -B output/build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build output/build --parallel
```

The executable is at `output/build/avar` (or `output/build\avar.exe` on Windows).

Build trees and packaged binaries are written under `output/` (git-ignored). See `CMakePresets.json` for common IDE presets.

### Build with embedded GUI (`avar-gui`)

This produces a single binary that serves the web UI from the daemon's HTTP channel:

```bash
cmake -S . -B output/build-gui -DCMAKE_BUILD_TYPE=Release -DAVAR_BUILD_GUI=ON -G Ninja
cmake --build output/build-gui --target avar-gui --parallel
```

The build runs `npm ci && npm run build` in `gui/` and embeds the result into the binary.

### Build the GUI separately

```bash
cd gui
npm install
npm run build          # static files in gui/dist/
npm run build:desktop  # Electron installers in gui/release/
```

See the [GUI documentation]({{ site.baseurl }}/gui/building.html) for development and packaging details.

### Run tests

```bash
ctest --test-dir output/build --output-on-failure
```

## Platform notes

### Linux

Install `cmake`, `ninja-build`, and a C23-capable compiler (`gcc` or `clang`). pthread is linked automatically.

### macOS

Use Xcode command-line tools or Homebrew LLVM. For daemon service installation, `launchd` integration is available via `avar daemon install --launchd`.

### Windows

`ws2_32` is linked automatically. Named pipes are used for CLI-to-daemon communication by default. Install as a Windows service with `avar daemon install --windows-service`.

## Verify the installation

```bash
avar --version
avar daemon ping
```

If the daemon is not running yet, `ping` reports that the daemon is unreachable — start it with `avar daemon start`.
