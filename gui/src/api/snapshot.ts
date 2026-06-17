import type { DownloadInfo, HealthInfo, QueueInfo, SnapshotPayload } from "./types";

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
  return {
    id: String(record.id ?? ""),
    filename: String(record.filename ?? record.url ?? "—"),
    url: record.url ? String(record.url) : undefined,
    status: String(record.status ?? "unknown"),
    queueId: record.queueId ? String(record.queueId) : undefined,
    bytesDownloaded: toNumber(record.bytesDownloaded),
    totalBytes: toNumber(record.totalBytes),
  };
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
  };
}

export { parseDownloadItem, parseQueueRecord };
