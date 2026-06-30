import { useCallback, useState } from "react";
import { useTranslation } from "react-i18next";
import type { DownloadInfo } from "@/api/types";
import {
  deleteDownloads,
  pauseDownloads,
  resumeDownloads,
  redownloadDownloads,
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
  canRedownload,
  isPaused,
  isCompleted,
} from "@/lib/downloadStatus";
import { copyDownloadToLocal } from "@/lib/copyDownloadToLocal";
import { openDownloadContainingFolder } from "@/lib/openDownloadContainingFolder";
import { openDownloadFile } from "@/lib/openDownloadFile";
import { appLogger } from "@/lib/appLogger";
import { isRemoteSession } from "@/lib/sessionRemote";
import { useCanOpenDownloadFile } from "@/hooks/useDownloadDoubleClickAction";
import { showConfirmDialog } from "@/lib/popup";
import { useConnectionStore } from "@/stores/connectionStore";
import { useConfigStore } from "@/stores/configStore";
import { useDataStore } from "@/stores/dataStore";

export function useDownloadActions() {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const activeSession = useConnectionStore((s) => s.activeSession);
  const localDownloadPath = useConfigStore((s) => s.config.localDownloadPath);
  const allDownloads = useDataStore((s) => s.downloads);
  const fileDownloadEnabled = useDataStore((s) => s.health?.fileDownloadEnabled === true);
  const [busy, setBusy] = useState(false);

  const remoteSessionActive =
    activeSession !== null && isRemoteSession(activeSession);
  const localCopyReady = localDownloadPath.trim().length > 0;
  const copyToLocalAvailable =
    remoteSessionActive && fileDownloadEnabled && localCopyReady;
  const copyToLocalVisible =
    remoteSessionActive && fileDownloadEnabled;
  const openFileVisible = useCanOpenDownloadFile();

  const withBusy = useCallback(
    async (actionName: string, action: () => Promise<void>) => {
      if (!client) {
        appLogger.gui.debug(`${actionName} skipped (no client)`);
        return;
      }
      appLogger.gui.debug(actionName);
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
      withBusy("Download pause", async () => {
        if (!client) {
          return;
        }
        await pauseDownloads(client, ids, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const resume = useCallback(
    (ids: string[]) =>
      withBusy("Download resume", async () => {
        if (!client) {
          return;
        }
        await resumeDownloads(client, ids, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const start = useCallback(
    (ids: string[]) =>
      withBusy("Download start", async () => {
        if (!client) {
          return;
        }
        await startDownloads(client, ids, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const stop = useCallback(
    (ids: string[]) =>
      withBusy("Download stop", async () => {
        if (!client) {
          return;
        }
        await stopDownloads(client, ids, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const remove = useCallback(
    (ids: string[], purgeFiles = false) =>
      withBusy("Download delete", async () => {
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
        appLogger.gui.debug("Download delete cancelled");
        return;
      }
      await remove(items.map((item) => item.id), result.checkboxChecked);
    },
    [remove, t],
  );

  const togglePause = useCallback(
    (download: DownloadInfo) =>
      withBusy("Download pause/resume toggle", async () => {
        if (!client) {
          return;
        }
        await togglePauseResume(client, download, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const toggleStart = useCallback(
    (download: DownloadInfo) =>
      withBusy("Download start/stop toggle", async () => {
        if (!client) {
          return;
        }
        await toggleStartStop(client, download, allDownloads);
      }),
    [allDownloads, client, withBusy],
  );

  const redownload = useCallback(
    (items: DownloadInfo[]) =>
      withBusy("Download redownload", async () => {
        if (!client) {
          return;
        }
        const result = await showConfirmDialog({
          title: t("download.redownloadConfirmTitle"),
          message:
            items.length === 1
              ? t("download.redownloadConfirm", { name: items[0].filename })
              : t("download.redownloadConfirmBatch", { count: items.length }),
          confirmLabel: t("download.redownload"),
          cancelLabel: t("common.cancel"),
        });
        if (!result.confirmed) {
          appLogger.gui.debug("Download redownload cancelled");
          return;
        }
        await redownloadDownloads(client, items, async (item) => {
          const details = await client.getDownloadDetails(item.id);
          return details.url;
        });
      }),
    [client, t, withBusy],
  );

  const copyToLocal = useCallback(
    (items: DownloadInfo[]) =>
      withBusy("Copy download to local", async () => {
        if (!activeSession || !copyToLocalAvailable) {
          appLogger.gui.debug("Copy to local skipped (unavailable)");
          return;
        }
        for (const item of items) {
          await copyDownloadToLocal({
            session: activeSession,
            download: item,
            localDownloadPath: localDownloadPath.trim(),
          });
        }
      }),
    [activeSession, copyToLocalAvailable, localDownloadPath, withBusy],
  );

  const openFile = useCallback(
    (items: DownloadInfo[]) =>
      withBusy("Open download file", async () => {
        if (!client || !openFileVisible) {
          appLogger.gui.debug("Open download file skipped (unavailable)");
          return;
        }
        for (const item of items) {
          if (!isCompleted(item.status)) {
            continue;
          }
          try {
            await openDownloadFile(client, item);
          } catch (error: unknown) {
            const detail = error instanceof Error ? error.message : String(error);
            appLogger.gui.warn("Failed to open download file", detail);
          }
        }
      }),
    [client, openFileVisible, withBusy],
  );

  const openContainingFolder = useCallback(
    (items: DownloadInfo[]) =>
      withBusy("Open download containing folder", async () => {
        if (!client || !openFileVisible) {
          appLogger.gui.debug("Open containing folder skipped (unavailable)");
          return;
        }
        for (const item of items) {
          if (!isCompleted(item.status)) {
            continue;
          }
          try {
            await openDownloadContainingFolder(client, item);
          } catch (error: unknown) {
            const detail = error instanceof Error ? error.message : String(error);
            appLogger.gui.warn("Failed to open download containing folder", detail);
          }
        }
      }),
    [client, openFileVisible, withBusy],
  );

  return {
    busy,
    pause,
    resume,
    start,
    stop,
    remove,
    removeWithConfirm,
    redownload,
    copyToLocal,
    openFile,
    openContainingFolder,
    remoteSessionActive,
    fileDownloadEnabled,
    localCopyReady,
    copyToLocalAvailable,
    copyToLocalVisible,
    openFileVisible,
    togglePause,
    toggleStart,
    canPause,
    canResume,
    canStart,
    canStop,
    canRedownload,
    isPaused,
  };
}
