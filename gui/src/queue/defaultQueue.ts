import type { QueueInfo } from "@/api/types";

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

export function withDefaultQueue(queues: QueueInfo[], defaultQueue: QueueInfo): QueueInfo[] {
  return [defaultQueue, ...queues];
}
