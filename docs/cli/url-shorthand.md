---
title: URL Shorthand
sort: 1
parent: CLI Reference
---

# URL Shorthand

Pass an HTTP or HTTPS URL directly to `avar` to queue a download without typing a subcommand.

## Syntax

```
avar <url> [--attached] [--queue=<name>] [--name=<filename>] [--proxy=<url>]
```

## Options

| Option | Description |
|--------|-------------|
| `--attached` | Run the download in the current process if the daemon is unreachable |
| `--queue=<name>` | Target queue by name (default queue if omitted) |
| `--name=<filename>` | Override the output filename |
| `--proxy=<url>` | Per-download proxy (`http://`, `https://`, or `socks5://`) |

## Examples

```bash
# Basic download
avar https://example.com/archive.zip

# Specify queue and filename
avar https://example.com/video.mp4 --queue=media --name=lecture-01.mp4

# Use a SOCKS5 proxy for this download
avar https://example.com/file.bin --proxy=socks5://127.0.0.1:1080

# Fall back to local download if daemon is down
avar https://example.com/file.zip --attached
```

## Supported schemes

Only **http** and **https** URLs are accepted. Other schemes (ftp, magnet, etc.) are not supported at this time.

## How it works

The URL shorthand routes through the daemon session layer. When the daemon is running, the download is queued remotely. With `--attached`, the CLI attempts a local in-process download if the daemon cannot be reached.

This is equivalent to:

```bash
avar dl <url> [options]
```
