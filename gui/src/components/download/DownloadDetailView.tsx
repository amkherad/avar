import { useTranslation } from "react-i18next";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import type { DownloadInfo } from "@/api/types";
import { formatBytePair, progressPercent } from "./format";

export interface DownloadDetailViewProps {
  download: DownloadInfo;
  onOpenPopup?: () => void;
  compact?: boolean;
}

function statusTone(status: string): "default" | "success" | "warning" | "danger" | "info" {
  switch (status) {
    case "downloading":
      return "info";
    case "completed":
      return "success";
    case "error":
      return "danger";
    case "paused":
      return "warning";
    default:
      return "default";
  }
}

export function DownloadDetailView({ download, onOpenPopup, compact }: DownloadDetailViewProps) {
  const { t } = useTranslation();
  const percent = progressPercent(download.bytesDownloaded, download.totalBytes);

  return (
    <div className={`avar-download-detail ${compact ? "avar-download-detail--compact" : ""}`}>
      <div className="avar-download-detail__header">
        <h3 className="avar-download-detail__filename">{download.filename}</h3>
        <Badge tone={statusTone(download.status)}>{download.status}</Badge>
      </div>

      <dl className="avar-download-detail__fields">
        <div className="avar-download-detail__field">
          <dt>{t("download.progress")}</dt>
          <dd>
            <div className="avar-progress" aria-hidden="true">
              <div className="avar-progress__bar" style={{ width: `${percent}%` }} />
            </div>
            <span className="avar-list__meta">
              {formatBytePair(download.bytesDownloaded, download.totalBytes)} ({percent}%)
            </span>
          </dd>
        </div>

        {download.url ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.url")}</dt>
            <dd className="avar-download-detail__url">{download.url}</dd>
          </div>
        ) : null}

        <div className="avar-download-detail__field">
          <dt>{t("download.id")}</dt>
          <dd className="avar-download-detail__mono">{download.id}</dd>
        </div>

        {download.queueId ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.queue")}</dt>
            <dd>{download.queueId}</dd>
          </div>
        ) : null}
      </dl>

      {onOpenPopup ? (
        <div className="avar-download-detail__actions">
          <Button size="sm" variant="secondary" onClick={onOpenPopup}>
            {t("download.openWindow")}
          </Button>
        </div>
      ) : null}
    </div>
  );
}
