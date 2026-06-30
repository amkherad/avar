import type { DownloadInfo, HealthInfo, QueueInfo, SystemStatsInfo, SnapshotPayload } from "./types";

function parseQueueRecord(item: unknown): QueueInfo {
  const record = (item ?? {}) as Record<string, unknown>;
  return {
    id: String(record.id ?? ""),
    name: String(record.name ?? ""),
    description: record.description ? String(record.description) : undefined,
    running: Boolean(record.started ?? record.running),
  };
}

function parseDownloadItem(item: unknown): DownloadInfo {
  const record = (item ?? {}) as Record<string, unknown>;
  const rawStatus = String(record.status ?? "unknown");
  const status = rawStatus === "failed" ? "error" : rawStatus;
  const maxRetriesRaw = record.maxRetries;
  let maxRetries: number | null | undefined;
  if (maxRetriesRaw === null) {
    maxRetries = null;
  } else if (maxRetriesRaw !== undefined) {
    maxRetries = toNumber(maxRetriesRaw);
  }

  return {
    id: String(record.id ?? ""),
    filename: String(record.filename ?? "—"),
    filenameInferred:
      record.filenameInferred === undefined
        ? undefined
        : Boolean(record.filenameInferred),
    url: record.url ? String(record.url) : undefined,
    referer: record.referer
      ? String(record.referer)
      : record.originalPage
        ? String(record.originalPage)
        : undefined,
    status,
    queueId: record.queueId ? String(record.queueId) : undefined,
    bytesDownloaded: toNumber(record.bytesDownloaded),
    totalBytes: toNumber(record.totalBytes),
    chunkSize: record.chunkSize !== undefined ? toNumber(record.chunkSize) : undefined,
    doneRanges: parseDoneRanges(record.doneRanges),
    errorCount: record.errorCount !== undefined ? toNumber(record.errorCount) : undefined,
    maxRetries,
    description: record.description ? String(record.description) : undefined,
    bytesPerSecond:
      record.bytesPerSecond !== undefined ? toNumber(record.bytesPerSecond) : undefined,
    destPath: record.destPath ? String(record.destPath) : undefined,
  };
}

function parseDoneRanges(value: unknown): Array<[number, number]> | undefined {
  if (!Array.isArray(value)) {
    return undefined;
  }

  const ranges: Array<[number, number]> = [];
  for (const item of value) {
    if (!Array.isArray(item) || item.length < 2) {
      continue;
    }
    ranges.push([toNumber(item[0]), toNumber(item[1])]);
  }

  return ranges.length > 0 ? ranges : undefined;
}

function toNumber(value: unknown): number {
  if (typeof value === "number") {
    return value;
  }
  if (typeof value === "string") {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : 0;
  }
  return 0;
}

function parseSystemStats(value: unknown): SystemStatsInfo | undefined {
  if (!value || typeof value !== "object") {
    return undefined;
  }
  const record = value as Record<string, unknown>;
  return {
    status: String(record.status ?? ""),
    diskTotalBytes: toNumber(record.diskTotalBytes),
    diskFreeBytes: toNumber(record.diskFreeBytes),
    memoryTotalBytes: toNumber(record.memoryTotalBytes),
    memoryUsedBytes: toNumber(record.memoryUsedBytes),
    memoryUsedPercent: toNumber(record.memoryUsedPercent),
    cpuUsagePercent: toNumber(record.cpuUsagePercent),
    networkRxBytesPerSec: toNumber(record.networkRxBytesPerSec),
    networkTxBytesPerSec: toNumber(record.networkTxBytesPerSec),
  };
}

export function isUnchangedStreamPayload(raw: unknown): boolean {
  if (!raw || typeof raw !== "object") {
    return false;
  }
  return (raw as Record<string, unknown>).type === "unchanged";
}

export function parseSnapshotPayload(raw: unknown): SnapshotPayload | null {
  if (!raw || typeof raw !== "object") {
    return null;
  }
  const record = raw as Record<string, unknown>;
  return {
    type: record.type ? String(record.type) : undefined,
    health: record.health as HealthInfo | undefined,
    queues: Array.isArray(record.queues)
      ? record.queues.map((item) => parseQueueRecord(item))
      : undefined,
    downloads: Array.isArray(record.downloads)
      ? record.downloads.map((item) => parseDownloadItem(item))
      : undefined,
    stats: parseSystemStats(record.stats),
  };
}

export function parseStreamStatsPayload(raw: unknown): SystemStatsInfo | null {
  if (!raw || typeof raw !== "object") {
    return null;
  }
  const record = raw as Record<string, unknown>;
  if (record.type !== "stats") {
    return null;
  }
  return parseSystemStats(record) ?? null;
}

export { parseDownloadItem, parseQueueRecord };
