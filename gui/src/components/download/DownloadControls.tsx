import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import {
  faPause,
  faPlay,
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
  isPaused,
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
    togglePause,
    toggleStart,
  } = useDownloadActions();

  const single = downloads.length === 1 ? downloads[0] : null;
  const ids = downloads.map((item) => item.id);

  const anyPausable = downloads.some((item) => canPause(item.status));
  const anyResumable = downloads.some((item) => canResume(item.status));
  const anyStartable = downloads.some((item) => canStart(item.status));
  const anyStoppable = downloads.some((item) => canStop(item.status));

  function handlePauseResume() {
    if (single) {
      void togglePause(single);
      return;
    }
    if (anyResumable) {
      void resume(ids);
    } else if (anyPausable) {
      void pause(ids);
    }
  }

  function handleStartStop() {
    if (single) {
      void toggleStart(single);
      return;
    }
    if (anyStoppable) {
      void stop(ids);
    } else if (anyStartable) {
      void start(ids);
    }
  }

  const showPause =
    downloads.length > 1 ? anyPausable || anyResumable : single && (canPause(single.status) || isPaused(single.status));
  const showStartStop =
    downloads.length > 1
      ? anyStartable || anyStoppable
      : single && (canStart(single.status) || canStop(single.status));

  const paused = single ? isPaused(single.status) : anyResumable && !anyPausable;

  return (
    <div className={`avar-download-controls ${className}`.trim()}>
      {showPause ? (
        <ShortcutButton
          size="sm"
          variant="secondary"
          loading={busy}
          shortcut="download.pause"
          registerShortcut={false}
          aria-label={paused ? t("download.resume") : t("download.pause")}
          onClick={() => void handlePauseResume()}
        >
          <FontAwesomeIcon icon={paused ? faPlay : faPause} />
        </ShortcutButton>
      ) : null}
      {showStartStop ? (
        <ShortcutButton
          size="sm"
          variant="secondary"
          loading={busy}
          shortcut={anyStoppable || (single && canStop(single.status)) ? "download.stop" : "download.start"}
          registerShortcut={false}
          aria-label={
            anyStoppable || (single && canStop(single.status))
              ? t("download.stop")
              : t("download.start")
          }
          onClick={() => void handleStartStop()}
        >
          <FontAwesomeIcon
            icon={anyStoppable || (single && canStop(single.status)) ? faStop : faPlay}
          />
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
