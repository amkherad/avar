/**
 * Default daemon config values for the GUI settings forms.
 * Keep in sync with docs/architecture/config-file.md and backend fallbacks in src/.
 */
export const DOWNLOAD_CONFIG_DEFAULTS = {
  "dm.tempPath": "",
  "dm.downloadPath": "",
  "dm.segmentation.enabled": "true",
  "dm.segmentation.strategy": "balanced",
  "dm.segmentation.concurrency": "4",
  "dm.segmentation.chunkSize": "262144",
  "dm.segmentation.minFileSize": "1048576",
  "dm.progress.sizeUnit": "MiB",
  "dm.progress.speedUnit": "MiB/s",
  "dm.progress.style": "segmented",
  "dm.proxy.enabled": "false",
  "dm.proxy.type": "http",
  "dm.proxy.host": "",
  "dm.proxy.port": "",
  "dm.proxy.username": "",
  "dm.proxy.password": "",
  "dm.proxy.noProxy": "",
} as const;

export const DOWNLOAD_CONFIG_FIELD_KEYS = [
  "dm.tempPath",
  "dm.downloadPath",
  "dm.segmentation.enabled",
  "dm.segmentation.strategy",
  "dm.segmentation.concurrency",
  "dm.segmentation.chunkSize",
  "dm.segmentation.minFileSize",
  "dm.progress.sizeUnit",
  "dm.progress.speedUnit",
  "dm.progress.style",
] as const satisfies readonly (keyof typeof DOWNLOAD_CONFIG_DEFAULTS)[];

export const DOWNLOAD_PROXY_CONFIG_KEYS = [
  "dm.proxy.enabled",
  "dm.proxy.type",
  "dm.proxy.host",
  "dm.proxy.port",
  "dm.proxy.username",
  "dm.proxy.password",
  "dm.proxy.noProxy",
] as const satisfies readonly (keyof typeof DOWNLOAD_CONFIG_DEFAULTS)[];

export const DAEMON_CONFIG_DEFAULTS = {
  "daemon.server.autoShutdown": "never",
  "daemon.server.autoShutdownIdleSeconds": "60",
  "log.file.enabled": "false",
  "log.file.path": "",
  "bookmarks.file.path": "",
  "daemon.server.fileDownload.enabled": "false",
  "daemon.server.fsBrowse.enabled": "false",
} as const;

export const DAEMON_CONFIG_FIELD_KEYS = [
  "daemon.server.autoShutdown",
  "daemon.server.autoShutdownIdleSeconds",
  "log.file.enabled",
  "log.file.path",
  "bookmarks.file.path",
  "daemon.server.fileDownload.enabled",
  "daemon.server.fsBrowse.enabled",
] as const satisfies readonly (keyof typeof DAEMON_CONFIG_DEFAULTS)[];

export type DownloadConfigKey = keyof typeof DOWNLOAD_CONFIG_DEFAULTS;
export type DaemonConfigKey = keyof typeof DAEMON_CONFIG_DEFAULTS;

export function configDefaultValue(
  defaults: Record<string, string>,
  key: string,
): string {
  return defaults[key] ?? "";
}

export function defaultDownloadConfigValues(): Record<DownloadConfigKey, string> {
  return { ...DOWNLOAD_CONFIG_DEFAULTS };
}

export function defaultDaemonConfigValues(): Record<DaemonConfigKey, string> {
  return { ...DAEMON_CONFIG_DEFAULTS };
}

export const DOWNLOAD_NUMERIC_CONFIG_KEYS = new Set<string>([
  "dm.segmentation.concurrency",
  "dm.segmentation.chunkSize",
  "dm.segmentation.minFileSize",
  "daemon.server.autoShutdownIdleSeconds",
]);

export function shouldPersistDaemonConfigValue(key: string, value: string): boolean {
  if (DOWNLOAD_NUMERIC_CONFIG_KEYS.has(key) && value.trim() === "") {
    return false;
  }
  return true;
}
