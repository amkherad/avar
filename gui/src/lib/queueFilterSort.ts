import type { QueueInfo } from "@/api/types";

export type QueueStatusFilter = "all" | "running" | "stopped";

export type QueueSortKey = "name" | "description" | "status" | "downloads" | null;
export type QueueSortDirection = "asc" | "desc";

export interface QueueSort {
  key: QueueSortKey;
  direction: QueueSortDirection;
}

function normalizeQuery(query: string): string {
  return query.trim().toLowerCase();
}

function queueSearchText(queue: QueueInfo, downloadCount: number): string {
  const status = queue.running ? "running" : "stopped";
  return [queue.id, queue.name, queue.description ?? "", status, String(downloadCount)]
    .join("\n")
    .toLowerCase();
}

export function filterQueuesBySearch(
  queues: QueueInfo[],
  query: string,
  downloadCounts: Record<string, number>,
): QueueInfo[] {
  const normalized = normalizeQuery(query);
  if (!normalized) {
    return queues;
  }
  return queues.filter((queue) =>
    queueSearchText(queue, downloadCounts[queue.id] ?? 0).includes(normalized),
  );
}

export function filterQueuesByStatus(
  queues: QueueInfo[],
  statusFilter: QueueStatusFilter,
): QueueInfo[] {
  if (statusFilter === "all") {
    return queues;
  }
  if (statusFilter === "running") {
    return queues.filter((queue) => queue.running);
  }
  return queues.filter((queue) => !queue.running);
}

export function sortQueues(
  queues: QueueInfo[],
  sort: QueueSort,
  downloadCounts: Record<string, number>,
): QueueInfo[] {
  if (!sort.key) {
    return queues;
  }

  const sorted = [...queues];
  const direction = sort.direction === "asc" ? 1 : -1;

  sorted.sort((a, b) => {
    if (sort.key === "name") {
      return a.name.localeCompare(b.name) * direction;
    }
    if (sort.key === "description") {
      const descA = a.description ?? "";
      const descB = b.description ?? "";
      return descA.localeCompare(descB) * direction;
    }
    if (sort.key === "status") {
      const rankA = a.readonly ? 2 : a.running ? 0 : 1;
      const rankB = b.readonly ? 2 : b.running ? 0 : 1;
      if (rankA !== rankB) {
        return (rankA - rankB) * direction;
      }
      return a.name.localeCompare(b.name) * direction;
    }
    const countA = downloadCounts[a.id] ?? 0;
    const countB = downloadCounts[b.id] ?? 0;
    if (countA !== countB) {
      return (countA - countB) * direction;
    }
    return a.name.localeCompare(b.name) * direction;
  });

  return sorted;
}
