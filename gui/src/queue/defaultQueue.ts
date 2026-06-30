import type { DownloadInfo, QueueInfo } from "@/api/types";
import { isActive } from "@/lib/downloadStatus";

/** Sentinel id for downloads with no assigned queue (null queueId). */
export const DEFAULT_QUEUE_ID = "__default__";

export function isDefaultQueue(id: string | null | undefined): boolean {
  return id === DEFAULT_QUEUE_ID;
}

export function createDefaultQueueInfo(name: string, description?: string): QueueInfo {
  return {
    id: DEFAULT_QUEUE_ID,
    name,
    description,
    running: false,
    readonly: true,
  };
}

export function defaultQueueDownloads(downloads: DownloadInfo[]): DownloadInfo[] {
  return downloads.filter((item) => !item.queueId);
}

export function isDefaultQueueRunning(downloads: DownloadInfo[]): boolean {
  return defaultQueueDownloads(downloads).some((item) => isActive(item.status));
}

/** Start/stop applies to the default queue and normal editable queues. */
export function queueHasLifecycleActions(queue: QueueInfo): boolean {
  return isDefaultQueue(queue.id) || !queue.readonly;
}

/** Modify, delete, and row selection apply only to normal queues. */
export function queueIsEditable(queue: QueueInfo): boolean {
  return !queue.readonly && !isDefaultQueue(queue.id);
}

export function withDefaultQueue(
  queues: QueueInfo[],
  defaultQueue: QueueInfo,
  downloads?: DownloadInfo[],
): QueueInfo[] {
  const resolved =
    downloads !== undefined
      ? { ...defaultQueue, running: isDefaultQueueRunning(downloads) }
      : defaultQueue;
  return [resolved, ...queues];
}
