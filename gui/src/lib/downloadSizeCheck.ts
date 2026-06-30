import type { DaemonClient } from "@/api/daemon";
import type { DownloadDetails, DownloadInfo } from "@/api/types";
import { appLogger } from "@/lib/appLogger";
import { fileSizesMismatch } from "@/lib/downloadLinkValidation";
import {
  promptDownloadSizeMismatch,
  type DownloadSizeMismatchChoice,
} from "@/lib/downloadSizeMismatchPrompt";
import { useDataStore } from "@/stores/dataStore";

export interface DownloadProbeInfo {
  totalBytes: number;
  httpStatus: number;
}

export type DownloadSizeCheckResult =
  | { ok: true }
  | { ok: false; reason: "probe_failed" }
  | { ok: false; reason: "mismatch"; choice: DownloadSizeMismatchChoice };

export async function probeDownloadUrl(
  client: DaemonClient,
  url: string,
  options: {
    downloadId?: string;
    referer?: string;
  } = {},
): Promise<DownloadProbeInfo | null> {
  try {
    const result = await client.probeDownloadUrl({
      url,
      id: options.downloadId,
      referer: options.referer,
    });
    if (result.exitCode !== 0) {
      return null;
    }
    return {
      totalBytes: result.totalBytes,
      httpStatus: result.httpStatus,
    };
  } catch {
    return null;
  }
}

export async function checkDownloadSizeCompatibility(
  client: DaemonClient,
  download: DownloadInfo,
  details: DownloadDetails | undefined,
  url: string,
  referer?: string,
): Promise<DownloadSizeCheckResult> {
  if (download.totalBytes <= 0) {
    return { ok: true };
  }

  const probe = await probeDownloadUrl(client, url, {
    downloadId: download.id,
    referer: referer ?? details?.referer ?? details?.originalPage,
  });
  if (!probe) {
    appLogger.gui.debug("Download size probe failed", download.id);
    return { ok: false, reason: "probe_failed" };
  }
  if (!fileSizesMismatch(download.totalBytes, probe.totalBytes)) {
    return { ok: true };
  }

  const choice = await promptDownloadSizeMismatch(download, {
    persistedBytes: download.totalBytes,
    probedBytes: probe.totalBytes,
  });
  appLogger.gui.debug("Download size mismatch choice", choice);
  return { ok: false, reason: "mismatch", choice };
}

export async function guardDownloadStartSize(
  client: DaemonClient,
  download: DownloadInfo,
  details: DownloadDetails | undefined,
): Promise<boolean> {
  if (!details?.url) {
    return true;
  }

  const check = await checkDownloadSizeCompatibility(client, download, details, details.url);
  if (check.ok) {
    return true;
  }
  if (check.reason === "probe_failed") {
    return true;
  }

  if (check.choice === "restart") {
    appLogger.gui.info("Download restarted after size mismatch", download.filename);
    await client.restartDownload(download.id);
    await useDataStore.getState().refresh();
  } else if (check.choice === "new" && details.url) {
    appLogger.gui.info("New download added after size mismatch", download.filename);
    await client.addDownload({
      url: details.url,
      queue: download.queueId,
      name: download.filename,
      referer: details.referer ?? details.originalPage,
      startNow: true,
      forceNew: true,
    });
    await useDataStore.getState().refresh();
  } else if (check.choice === "cancel") {
    appLogger.gui.debug("Download start cancelled after size mismatch", download.filename);
  }

  return false;
}
