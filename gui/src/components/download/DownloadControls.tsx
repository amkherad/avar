import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import {
  faDownload,
  faPause,
  faPlay,
  faRotateRight,
  faStop,
  faTrash,
} from "@fortawesome/free-solid-svg-icons";
import type { DownloadInfo } from "@/api/types";
import { ShortcutButton } from "@/components/ui/ShortcutButton";
import { useDownloadActions } from "@/hooks/useDownloadActions";
import {
  canPause,
  canResume,
  canStart,
  canStop,
  canRedownload,
  isCompleted,
} from "@/lib/downloadStatus";

export interface DownloadControlsProps {
  downloads: DownloadInfo[];
  className?: string;
}

export function DownloadControls({ downloads, className = "" }: DownloadControlsProps) {
  const { t } = useTranslation();
  const {
    busy,
    pause,
    resume,
    start,
    stop,
    removeWithConfirm,
    redownload,
    copyToLocal,
    copyToLocalAvailable,
    copyToLocalVisible,
  } = useDownloadActions();

  const ids = downloads.map((item) => item.id);

  const anyPausable = downloads.some((item) => canPause(item.status));
  const anyResumable = downloads.some((item) => canResume(item.status));
  const anyStartable = downloads.some((item) => canStart(item.status));
  const anyStoppable = downloads.some((item) => canStop(item.status));
  const anyRedownloadable = downloads.some((item) => canRedownload(item.status));
  const anyCopyToLocal =
    copyToLocalVisible && downloads.some((item) => isCompleted(item.status));

  return (
    <div className={`avar-download-controls ${className}`.trim()}>
      {anyStartable ? (
        <ShortcutButton
          size="sm"
          variant="secondary"
          loading={busy}
          shortcut="download.start"
          registerShortcut={false}
          aria-label={t("download.start")}
          onClick={() => void start(ids)}
        >
          <FontAwesomeIcon icon={faPlay} />
        </ShortcutButton>
      ) : null}
      {anyStoppable ? (
        <ShortcutButton
          size="sm"
          variant="secondary"
          loading={busy}
          shortcut="download.stop"
          registerShortcut={false}
          aria-label={t("download.stop")}
          onClick={() => void stop(ids)}
        >
          <FontAwesomeIcon icon={faStop} />
        </ShortcutButton>
      ) : null}
      {anyPausable ? (
        <ShortcutButton
          size="sm"
          variant="secondary"
          loading={busy}
          shortcut="download.pause"
          registerShortcut={false}
          aria-label={t("download.pause")}
          onClick={() => void pause(ids)}
        >
          <FontAwesomeIcon icon={faPause} />
        </ShortcutButton>
      ) : null}
      {anyResumable ? (
        <ShortcutButton
          size="sm"
          variant="secondary"
          loading={busy}
          shortcut="download.pause"
          registerShortcut={false}
          aria-label={t("download.resume")}
          onClick={() => void resume(ids)}
        >
          <FontAwesomeIcon icon={faPlay} />
        </ShortcutButton>
      ) : null}
      {anyRedownloadable ? (
        <ShortcutButton
          size="sm"
          variant="secondary"
          loading={busy}
          registerShortcut={false}
          aria-label={t("download.redownload")}
          onClick={() => void redownload(downloads)}
        >
          <FontAwesomeIcon icon={faRotateRight} />
        </ShortcutButton>
      ) : null}
      {anyCopyToLocal ? (
        <ShortcutButton
          size="sm"
          variant="secondary"
          loading={busy}
          registerShortcut={false}
          disabled={!copyToLocalAvailable}
          aria-label={t("download.copyToLocal")}
          onClick={() => void copyToLocal(downloads.filter((item) => isCompleted(item.status)))}
        >
          <FontAwesomeIcon icon={faDownload} />
        </ShortcutButton>
      ) : null}
      <ShortcutButton
        size="sm"
        variant="danger"
        loading={busy}
        shortcut="download.delete"
        registerShortcut={false}
        aria-label={t("download.delete")}
        onClick={() => void removeWithConfirm(downloads)}
      >
        <FontAwesomeIcon icon={faTrash} />
      </ShortcutButton>
    </div>
  );
}
