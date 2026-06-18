import type { TFunction } from "i18next";
import type { DownloadStatus } from "@/api/types";

const KNOWN_STATUSES = new Set<string>([
  "queued",
  "downloading",
  "paused",
  "completed",
  "error",
  "failed",
  "cancelled",
  "stopped",
]);

export function formatDownloadStatus(status: DownloadStatus, t: TFunction): string {
  if (KNOWN_STATUSES.has(status)) {
    return t(`download.statusValues.${status}`);
  }
  return status;
}
