import { useCallback, useState } from "react";
import { useTranslation } from "react-i18next";
import type { DownloadInfo } from "@/api/types";
import {
  deleteDownloads,
  pauseDownloads,
  resumeDownloads,
  startDownloads,
  stopDownloads,
  togglePauseResume,
  toggleStartStop,
} from "@/lib/downloadActions";
import {
  canPause,
  canResume,
  canStart,
  canStop,
  isPaused,
} from "@/lib/downloadStatus";
import { showConfirmDialog } from "@/lib/popup";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";

export function useDownloadActions() {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const allDownloads = useDataStore((s) => s.downloads);
  const [busy, setBusy] = useState(false);

  const withBusy = useCallback(
    async (action: () => Promise<void>) => {
      if (!client) {
        return;
      }
      setBusy(true);
      try {
        await action();
      } finally {
        setBusy(false);
      }
    },
    [client],
  );

  const pause = useCallback(
    (ids: string[]) =>
      withBusy(async () => {
        if (!client) {
          return;
        }
        await pauseDownloads(client, ids, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const resume = useCallback(
    (ids: string[]) =>
      withBusy(async () => {
        if (!client) {
          return;
        }
        await resumeDownloads(client, ids, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const start = useCallback(
    (ids: string[]) =>
      withBusy(async () => {
        if (!client) {
          return;
        }
        await startDownloads(client, ids, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const stop = useCallback(
    (ids: string[]) =>
      withBusy(async () => {
        if (!client) {
          return;
        }
        await stopDownloads(client, ids, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const remove = useCallback(
    (ids: string[], purgeFiles = false) =>
      withBusy(async () => {
        if (!client) {
          return;
        }
        await deleteDownloads(client, ids, purgeFiles);
      }),
    [client, withBusy],
  );

  const removeWithConfirm = useCallback(
    async (items: DownloadInfo[]) => {
      if (items.length === 0) {
        return;
      }
      const message =
        items.length === 1
          ? t("download.deleteConfirm", { name: items[0].filename })
          : t("download.deleteConfirmBatch", { count: items.length });
      const result = await showConfirmDialog({
        title: t("download.deleteConfirmTitle"),
        message,
        confirmLabel: t("download.delete"),
        cancelLabel: t("common.cancel"),
        checkboxLabel: t("download.deleteFiles"),
        checkboxDefault: false,
      });
      if (!result.confirmed) {
        return;
      }
      await remove(items.map((item) => item.id), result.checkboxChecked);
    },
    [remove, t],
  );

  const togglePause = useCallback(
    (download: DownloadInfo) =>
      withBusy(async () => {
        if (!client) {
          return;
        }
        await togglePauseResume(client, download, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const toggleStart = useCallback(
    (download: DownloadInfo) =>
      withBusy(async () => {
        if (!client) {
          return;
        }
        await toggleStartStop(client, download, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  return {
    busy,
    pause,
    resume,
    start,
    stop,
    remove,
    removeWithConfirm,
    togglePause,
    toggleStart,
    canPause,
    canResume,
    canStart,
    canStop,
    isPaused,
  };
}
