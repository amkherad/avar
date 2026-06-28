import type { DownloadInfo } from "@/api/types";

/**
 * Download search — backend module
 *
 * When adding new properties to DownloadInfo, register them in
 * `DOWNLOAD_SEARCH_EXTRACTORS` below so they are included in search.
 * Do not scatter search logic across components; keep it here.
 */

export type DownloadSearchExtractor = (download: DownloadInfo) => string[];

/**
 * Extract searchable text from a download item.
 * Add a new extractor entry when DownloadInfo gains new fields.
 */
export const DOWNLOAD_SEARCH_EXTRACTORS: DownloadSearchExtractor[] = [
  (d) => [d.id],
  (d) => [d.filename],
  (d) => [d.status],
  (d) => [d.queueId ?? ""],
  (d) => [String(d.bytesDownloaded), String(d.totalBytes)],
  // ── Add new DownloadInfo field extractors above this line ──
];

function normalizeQuery(query: string): string {
  return query.trim().toLowerCase();
}

function collectSearchText(download: DownloadInfo): string {
  const parts: string[] = [];
  for (const extract of DOWNLOAD_SEARCH_EXTRACTORS) {
    parts.push(...extract(download));
  }
  return parts.join("\n").toLowerCase();
}

export function downloadMatchesSearch(download: DownloadInfo, query: string): boolean {
  const normalized = normalizeQuery(query);
  if (!normalized) {
    return true;
  }
  return collectSearchText(download).includes(normalized);
}

export function filterDownloadsBySearch(
  downloads: DownloadInfo[],
  query: string,
): DownloadInfo[] {
  const normalized = normalizeQuery(query);
  if (!normalized) {
    return downloads;
  }
  return downloads.filter((d) => downloadMatchesSearch(d, normalized));
}
