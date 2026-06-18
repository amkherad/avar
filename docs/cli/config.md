---
title: config
sort: 4
parent: CLI Reference
---

# config

Read and write Avar configuration values.

```
avar config <subcommand> [options]
```

Configuration is stored in `config.json`. See [Configuration overview]({{ site.baseurl }}/getting-started/configuration-overview.html) for file locations.

## Subcommands

### get

```
avar config get <name> [--format=json] [--defaultValue=<value>]
```

Print the value of a configuration key.

```bash
avar config get dm.downloadPath
avar config get daemon.session.mode --format=json
```

### set

```
avar config set <name> <value>
```

Set a configuration key. Values are parsed according to their type (string, number, boolean).

```bash
avar config set dm.downloadPath /home/user/Downloads
avar config set daemon.session.mode remote
avar config set dm.proxy.enabled true
```

### reset

```
avar config reset <name>
```

Reset a single key to its default value.

### reset-all

```
avar config reset-all
```

Reset all configuration to defaults.

### save

```
avar config save <path>
```

Export the current configuration to a file.

### load

```
avar config load <path>
```

Load configuration from a file, replacing the current in-memory config.

## Key naming

Keys use dot notation matching the JSON structure:

| Key | Description |
|-----|-------------|
| `dm.downloadPath` | Default download directory |
| `dm.tempPath` | Temporary file directory |
| `dm.segmentation.enabled` | Enable parallel segments |
| `dm.segmentation.concurrency` | Number of parallel segments |
| `dm.proxy.enabled` | Enable global proxy |
| `dm.proxy.type` | `http`, `https`, or `socks5` |
| `daemon.session.mode` | `local` or `remote` |
| `daemon.session.transport` | `pipe`, `unix`, or `http` |
| `daemon.server.channels.http.port` | HTTP API port |

See [Configuration file]({{ site.baseurl }}/architecture/config-file.html) for the complete schema.

## Environment overrides

Set `avar.<key>` environment variables to override config values at runtime:

```bash
export avar.daemon.session.mode=remote
export avar.daemon.server.channels.http.port=9000
avar dl https://example.com/file.zip
```

Environment variables take precedence over `config.json`.
