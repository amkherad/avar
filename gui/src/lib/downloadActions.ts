import type { DaemonClient } from "@/api/daemon";
import type { DownloadInfo } from "@/api/types";
import { appLogger } from "@/lib/appLogger";
import {
  canPause,
  canResume,
  canStart,
  canStop,
  canRedownload,
  isPaused,
} from "@/lib/downloadStatus";
import { useDataStore } from "@/stores/dataStore";
import { useConnectionStore } from "@/stores/connectionStore";

export type DownloadActionKind = "pause" | "resume" | "start" | "stop" | "delete";

export interface DownloadActionResult {
  succeeded: string[];
  failed: string[];
}

async function runForIds(
  ids: string[],
  label: string,
  action: (id: string) => Promise<void>,
  describe?: (id: string) => string | undefined,
): Promise<DownloadActionResult> {
  const succeeded: string[] = [];
  const failed: string[] = [];

  for (const id of ids) {
    if (!id) {
      continue;
    }
    try {
      await action(id);
      succeeded.push(id);
      appLogger.gui.info(`Download ${label}`, describe?.(id) ?? id);
    } catch (err) {
      failed.push(id);
      appLogger.gui.error(
        label,
        err instanceof Error ? err.message : undefined,
      );
    }
  }

  if (succeeded.length > 0) {
    await useDataStore.getState().refresh();
  }

  return { succeeded, failed };
}

export async function pauseDownloads(
  client: DaemonClient,
  ids: string[],
  downloads: DownloadInfo[],
): Promise<DownloadActionResult> {
  const eligible = ids.filter((id) => {
    const item = downloads.find((d) => d.id === id);
    return item && canPause(item.status);
  });
  return runForIds(
    eligible,
    "pause",
    (id) => client.pauseDownload(id),
    (id) => downloads.find((d) => d.id === id)?.filename,
  );
}

export async function resumeDownloads(
  client: DaemonClient,
  ids: string[],
  downloads: DownloadInfo[],
): Promise<DownloadActionResult> {
  const eligible = ids.filter((id) => {
    const item = downloads.find((d) => d.id === id);
    return item && canResume(item.status);
  });
  return runForIds(
    eligible,
    "resume",
    (id) => client.resumeDownload(id),
    (id) => downloads.find((d) => d.id === id)?.filename,
  );
}

export async function startDownloads(
  client: DaemonClient,
  ids: string[],
  downloads: DownloadInfo[],
): Promise<DownloadActionResult> {
  const eligible = ids.filter((id) => {
    const item = downloads.find((d) => d.id === id);
    return item && canStart(item.status);
  });
  return runForIds(
    eligible,
    "start",
    (id) => client.startDownload(id),
    (id) => downloads.find((d) => d.id === id)?.filename,
  );
}

export async function stopDownloads(
  client: DaemonClient,
  ids: string[],
  downloads: DownloadInfo[],
): Promise<DownloadActionResult> {
  const eligible = ids.filter((id) => {
    const item = downloads.find((d) => d.id === id);
    return item && canStop(item.status);
  });
  return runForIds(
    eligible,
    "stop",
    (id) => client.stopDownload(id),
    (id) => downloads.find((d) => d.id === id)?.filename,
  );
}

export async function deleteDownloads(
  client: DaemonClient,
  ids: string[],
  purgeFiles = false,
): Promise<DownloadActionResult> {
  const result = await runForIds(
    ids,
    "delete",
    (id) => client.removeDownload(id, purgeFiles),
    (id) => useDataStore.getState().downloads.find((d) => d.id === id)?.filename,
  );
  if (result.succeeded.length > 0) {
    const state = useDataStore.getState();
    const remaining = state.selectedDownloadIds.filter(
      (id) => !result.succeeded.includes(id),
    );
    state.setSelectedDownloadIds(remaining);
    if (
      state.selectedDownloadId &&
      result.succeeded.includes(state.selectedDownloadId)
    ) {
      state.setSelectedDownloadId(remaining[0] ?? null);
    }
  }
  return result;
}

export async function updateDownloadUrl(
  client: DaemonClient,
  id: string,
  url: string,
): Promise<void> {
  const trimmed = url.trim();
  if (!trimmed) {
    throw new Error("URL is required");
  }
  await client.setDownloadUrl(id, trimmed);
  await useDataStore.getState().refresh();
  appLogger.gui.info("Download URL updated", id);
}

export async function redownloadDownloads(
  client: DaemonClient,
  items: DownloadInfo[],
  resolveUrl: (item: DownloadInfo) => Promise<string | undefined>,
): Promise<DownloadActionResult> {
  const eligible = items.filter((item) => canRedownload(item.status));
  const succeeded: string[] = [];
  const failed: string[] = [];

  for (const item of eligible) {
    try {
      const url = await resolveUrl(item);
      if (!url) {
        failed.push(item.id);
        continue;
      }
      await client.removeDownload(item.id, true);
      await client.addDownload({
        url,
        queue: item.queueId,
        name: item.filename,
        startNow: true,
      });
      succeeded.push(item.id);
      appLogger.gui.info("Download redownloaded", item.filename);
    } catch (err) {
      failed.push(item.id);
      appLogger.gui.error(
        "redownload",
        err instanceof Error ? err.message : undefined,
      );
    }
  }

  if (succeeded.length > 0) {
    await useDataStore.getState().refresh();
  }

  return { succeeded, failed };
}

export function togglePauseResume(
  client: DaemonClient,
  download: DownloadInfo,
  downloads: DownloadInfo[],
): Promise<DownloadActionResult> {
  if (isPaused(download.status)) {
    return resumeDownloads(client, [download.id], downloads);
  }
  return pauseDownloads(client, [download.id], downloads);
}

export function toggleStartStop(
  client: DaemonClient,
  download: DownloadInfo,
  downloads: DownloadInfo[],
): Promise<DownloadActionResult> {
  if (canStop(download.status)) {
    return stopDownloads(client, [download.id], downloads);
  }
  if (canStart(download.status)) {
    return startDownloads(client, [download.id], downloads);
  }
  return Promise.resolve({ succeeded: [], failed: [] });
}

export function getClientOrNull(): DaemonClient | null {
  return useConnectionStore.getState().client;
}
