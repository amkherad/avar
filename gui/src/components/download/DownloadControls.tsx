import { useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import {
  faPause,
  faPlay,
  faStop,
  faTrash,
} from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import type { DownloadInfo } from "@/api/types";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";
import { showConfirmDialog } from "@/lib/popup";
import { appLogger } from "@/lib/appLogger";

export interface DownloadControlsProps {
  download: DownloadInfo;
  className?: string;
}

function isPaused(status: string): boolean {
  return status === "paused";
}

function isActive(status: string): boolean {
  return status === "downloading" || status === "queued";
}

function canStart(status: string): boolean {
  return status === "paused" || status === "queued" || status === "error" || status === "cancelled";
}

function canStop(status: string): boolean {
  return status === "downloading" || status === "queued";
}

export function DownloadControls({ download, className = "" }: DownloadControlsProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [busy, setBusy] = useState(false);

  async function runAction(
    label: string,
    action: () => Promise<void>,
    refresh = true,
  ) {
    if (!client || !download.id) {
      return;
    }
    setBusy(true);
    appLogger.gui.debug(`Download action: ${label}`, download.id);
    try {
      await action();
      appLogger.gui.info(label, download.filename);
      if (refresh) {
        await useDataStore.getState().refresh();
      }
    } catch (err) {
      appLogger.gui.error(
        label,
        err instanceof Error ? err.message : undefined,
      );
    } finally {
      setBusy(false);
    }
  }

  async function handlePauseResume() {
    if (isPaused(download.status)) {
      await runAction(t("download.resume"), () => client!.resumeDownload(download.id));
    } else {
      await runAction(t("download.pause"), () => client!.pauseDownload(download.id));
    }
  }

  async function handleStartStop() {
    if (canStop(download.status)) {
      await runAction(t("download.stop"), () => client!.stopDownload(download.id));
    } else if (canStart(download.status)) {
      await runAction(t("download.start"), () => client!.startDownload(download.id));
    }
  }

  async function handleDelete() {
    const confirmed = await showConfirmDialog({
      title: t("download.deleteConfirmTitle"),
      message: t("download.deleteConfirm", { name: download.filename }),
      confirmLabel: t("download.delete"),
      cancelLabel: t("common.cancel"),
    });
    if (!confirmed) {
      appLogger.gui.debug("Download delete cancelled", download.id);
      return;
    }
    await runAction(t("download.delete"), () => client!.removeDownload(download.id), true);
    useDataStore.getState().setSelectedDownloadId(null);
  }

  const paused = isPaused(download.status);
  const showPause = isActive(download.status) || paused;
  const showStartStop = canStart(download.status) || canStop(download.status);

  return (
    <div className={`avar-download-controls ${className}`.trim()}>
      {showPause ? (
        <Button
          size="sm"
          variant="secondary"
          loading={busy}
          aria-label={paused ? t("download.resume") : t("download.pause")}
          title={paused ? t("download.resume") : t("download.pause")}
          onClick={() => void handlePauseResume()}
        >
          <FontAwesomeIcon icon={paused ? faPlay : faPause} />
        </Button>
      ) : null}
      {showStartStop ? (
        <Button
          size="sm"
          variant="secondary"
          loading={busy}
          aria-label={canStop(download.status) ? t("download.stop") : t("download.start")}
          title={canStop(download.status) ? t("download.stop") : t("download.start")}
          onClick={() => void handleStartStop()}
        >
          <FontAwesomeIcon icon={canStop(download.status) ? faStop : faPlay} />
        </Button>
      ) : null}
      <Button
        size="sm"
        variant="danger"
        loading={busy}
        aria-label={t("download.delete")}
        title={t("download.delete")}
        onClick={() => void handleDelete()}
      >
        <FontAwesomeIcon icon={faTrash} />
      </Button>
    </div>
  );
}
