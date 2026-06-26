# Avar

[![CI](https://github.com/amkherad/avar/actions/workflows/ci.yml/badge.svg)](https://github.com/amkherad/avar/actions/workflows/ci.yml)

A cross-platform download manager written in **C23**, with a CLI, background daemon, React GUI, and browser extensions.

[Documentation](https://github.com/amkherad/avar/tree/main/docs) · [Report a bug](https://github.com/amkherad/avar/issues) · [Releases](https://github.com/amkherad/avar/releases)

## Features

- **HTTP/HTTPS downloads** — segmented parallel transfers, resume, and HLS (`.m3u8`) support
- **Queues** — named queues with concurrency limits and per-queue output paths
- **Daemon mode** — background service with JSON-RPC over HTTP, named pipe, or Unix socket
- **CLI** — script-friendly commands for downloads, queues, configuration, and daemon control
- **GUI** — web SPA and Electron desktop app with sessions, live progress, and i18n (English / Persian)
- **Browser extensions** — site-agnostic media capture for Chromium and Firefox
- **Cross-platform** — Linux, macOS, and Windows; systemd, launchd, and Windows service install

## Quick start

### Prerequisites

- CMake 4.1+
- C23-capable compiler (GCC, Clang, or MSVC)
- Git with submodules
- [Ninja](https://ninja-build.org/) (recommended)

### Build

```bash
git clone --recursive https://github.com/amkherad/avar.git
cd avar

cmake -S . -B output/build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build output/build --parallel
```

All CMake build trees, packaged binaries, and coverage reports live under `output/` (git-ignored). IDEs can use the presets in `CMakePresets.json` (`debug`, `release`, `gui`, `coverage`, `all`).

### Run

```bash
# Start the daemon (HTTP API on port 8000)
./output/build/avar daemon start --http --port=8000

# Download a file
./output/build/avar https://example.com/file.zip

# Check daemon health
./output/build/avar daemon ping
```

Pre-built binaries for tagged releases are published on the [Releases](https://github.com/amkherad/avar/releases) page.

## GUI

The React front end lives in [`gui/`](gui/). It connects to the daemon over HTTP JSON-RPC.

```bash
avar daemon start --http --port=8000

cd gui
npm install
npm run dev          # http://localhost:56000
npm run dev:desktop  # Electron + Vite
```

Production builds:

```bash
npm run build              # static files → gui/dist/
npm run build:desktop      # installers → gui/release/
```

Build a single binary with the UI embedded:

```bash
cmake -S . -B output/build-gui -DCMAKE_BUILD_TYPE=Release -DAVAR_BUILD_GUI=ON -G Ninja
cmake --build output/build-gui --target avar-gui --parallel
```

## Browser extensions

Extensions in [`extensions/`](extensions/) send media links from web pages to Avar via a local bridge.

1. Start the daemon and open the GUI.
2. Go to **Settings → Browser integration** and install the extension for your browser.
3. Load the unpacked folder (`extensions/chromium/` or `extensions/firefox/`).

See [`extensions/README.md`](extensions/README.md) for details.

## Project layout

```
avar/
├── src/           # C backend — CLI, daemon, download engine
├── gui/           # React SPA + Electron shell
├── extensions/    # Chromium and Firefox extensions
├── docs/          # Jekyll documentation site
├── scripts/       # Build and automation scripts
├── output/        # Build trees and packaged binaries (git-ignored)
└── tests/         # C unit tests (doctest)
```

## Documentation

Full user documentation is in [`docs/`](docs/):

| Section | Topics |
|---------|--------|
| [Getting started](docs/getting-started/) | Installation, quick start, configuration |
| [CLI reference](docs/cli/) | `download`, `queue`, `config`, `daemon` |
| [Architecture](docs/architecture/) | Daemon, download engine, queues, `config.json` |
| [GUI](docs/gui/) | Sessions, downloads, settings, shortcuts |
| [Extensions](docs/extensions/) | Installation, usage, bridge protocol |

Preview locally:

```bash
cd docs
bundle install
bundle exec jekyll serve
```

## Development

```bash
# Run tests (AddressSanitizer / UBSan enabled in CI)
ctest --test-dir output/build --output-on-failure
```

CI builds `avar`, the GUI, and `avar-gui` on every push to `main` / `master`. Tagged releases (`v*`) publish installers for Windows (`.exe`), Linux (`.deb`), and macOS (`.dmg`) on `x86_64` and `arm64`, including all-in-one `avar-gui-*` builds with embedded Electron.

## Contributing

Contributions are welcome. Please open an [issue](https://github.com/amkherad/avar/issues) to discuss larger changes before submitting a pull request.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-change`)
3. Commit your changes with a clear message
4. Push and open a pull request against `main`

## Author

**Ali Kherad** — [github.com/amkherad](https://github.com/amkherad)

## Acknowledgments

Built with [mbedtls](https://github.com/Mbed-TLS/mbedtls), [mongoose](https://github.com/cesanta/mongoose), [cJSON](https://github.com/DaveGamble/cJSON), and [argtable3](https://www.argtable.org/).
