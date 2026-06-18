import type { DownloadInfo } from "@/api/types";
import { progressPercent } from "@/components/download/format";

export type DownloadStatusFilter = "all" | DownloadInfo["status"];

export type DownloadSortKey = "status" | "progress" | null;
export type DownloadSortDirection = "asc" | "desc";

export interface DownloadSort {
  key: DownloadSortKey;
  direction: DownloadSortDirection;
}

const STATUS_ORDER: Record<string, number> = {
  downloading: 0,
  queued: 1,
  paused: 2,
  error: 3,
  failed: 3,
  cancelled: 4,
  stopped: 5,
  completed: 6,
};

function statusRank(status: string): number {
  return STATUS_ORDER[status] ?? 99;
}

export function filterDownloadsByStatus(
  downloads: DownloadInfo[],
  statusFilter: DownloadStatusFilter,
): DownloadInfo[] {
  if (statusFilter === "all") {
    return downloads;
  }
  return downloads.filter((item) => item.status === statusFilter);
}

export function sortDownloads(
  downloads: DownloadInfo[],
  sort: DownloadSort,
): DownloadInfo[] {
  if (!sort.key) {
    return downloads;
  }

  const sorted = [...downloads];
  const direction = sort.direction === "asc" ? 1 : -1;

  sorted.sort((a, b) => {
    if (sort.key === "status") {
      const diff = statusRank(a.status) - statusRank(b.status);
      if (diff !== 0) {
        return diff * direction;
      }
      return a.filename.localeCompare(b.filename) * direction;
    }

    const progressA = progressPercent(a.bytesDownloaded, a.totalBytes);
    const progressB = progressPercent(b.bytesDownloaded, b.totalBytes);
    if (progressA !== progressB) {
      return (progressA - progressB) * direction;
    }
    return a.filename.localeCompare(b.filename) * direction;
  });

  return sorted;
}

export function collectDownloadStatuses(downloads: DownloadInfo[]): string[] {
  const statuses = new Set<string>();
  for (const item of downloads) {
    if (item.status) {
      statuses.add(item.status);
    }
  }
  return Array.from(statuses).sort((a, b) => statusRank(a) - statusRank(b));
}
