import { formatBytePair, progressPercent } from "./format";
import { Badge } from "@/components/ui/Badge";
import { TruncateWithTooltip } from "@/components/ui/TruncateWithTooltip";
import type { DownloadInfo } from "@/api/types";
import { formatDownloadStatus } from "@/lib/downloadStatusLabel";
import { useTranslation } from "react-i18next";

export interface DownloadProgressProps {
  download: DownloadInfo;
}

export interface DownloadRowProps extends DownloadProgressProps {
  selected?: boolean;
  onSelect?: (event?: React.MouseEvent) => void;
  onOpen?: () => void;
  onContextMenu?: (event: React.MouseEvent) => void;
}

function statusTone(status: string): "default" | "success" | "warning" | "danger" | "info" {
  switch (status) {
    case "downloading":
      return "info";
    case "completed":
      return "success";
    case "error":
    case "failed":
      return "danger";
    case "paused":
      return "warning";
    default:
      return "default";
  }
}

export function DownloadProgress({ download }: DownloadProgressProps) {
  const percent = progressPercent(download.bytesDownloaded, download.totalBytes);
  const progressText = `${formatBytePair(download.bytesDownloaded, download.totalBytes)} (${percent}%)`;

  return (
    <div className="avar-download-progress">
      <div className="avar-progress" aria-hidden="true">
        <div className="avar-progress__bar" style={{ width: `${percent}%` }} />
      </div>
      <TruncateWithTooltip text={progressText} className="avar-list__meta" />
    </div>
  );
}

export function DownloadRow({
  download,
  selected = false,
  onSelect,
  onOpen,
  onContextMenu,
}: DownloadRowProps) {
  const { t } = useTranslation();

  return (
    <div
      className={`avar-download-row ${selected ? "avar-download-row--selected" : ""}`}
      role="button"
      tabIndex={0}
      onClick={(event) => onSelect?.(event)}
      onDoubleClick={() => onOpen?.()}
      onContextMenu={(event) => {
        event.preventDefault();
        onContextMenu?.(event);
      }}
      onKeyDown={(e) => {
        if (e.key === "Enter") {
          onSelect?.();
        }
      }}
    >
      <div>
        <TruncateWithTooltip text={download.filename} className="avar-list__title" />
        {download.url ? (
          <TruncateWithTooltip text={download.url} className="avar-list__meta" />
        ) : null}
        <DownloadProgress download={download} />
      </div>
      <Badge tone={statusTone(download.status)}>
        {formatDownloadStatus(download.status, t)}
      </Badge>
    </div>
  );
}
