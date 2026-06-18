import { useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faCopy } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { CopyButton } from "@/components/ui/CopyButton";
import type { DownloadInfo } from "@/api/types";
import { formatDownloadStatus } from "@/lib/downloadStatusLabel";
import { buildDownloadCurl, copyTextToClipboard } from "@/lib/curlCommand";
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
  const [copied, setCopied] = useState(false);

  async function handleCopyCurl() {
    const command = buildDownloadCurl(download);
    const ok = await copyTextToClipboard(command);
    if (ok) {
      setCopied(true);
      window.setTimeout(() => setCopied(false), 2000);
    }
  }

  return (
    <div className={`avar-download-detail ${compact ? "avar-download-detail--compact" : ""}`}>
      <div className="avar-download-detail__header">
        <div className="avar-download-detail__filename-row">
          <h3 className="avar-download-detail__filename">{download.filename}</h3>
          {download.filename ? (
            <CopyButton text={download.filename} label={t("download.copyFilename")} />
          ) : null}
        </div>
        <Badge tone={statusTone(download.status)}>
          {formatDownloadStatus(download.status, t)}
        </Badge>
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
            <dd className="avar-download-detail__value-row">
              <span className="avar-download-detail__url">{download.url}</span>
              <CopyButton text={download.url} label={t("download.copyUrl")} />
            </dd>
          </div>
        ) : null}

        <div className="avar-download-detail__field">
          <dt>{t("download.id")}</dt>
          <dd className="avar-download-detail__value-row">
            <span className="avar-download-detail__mono">{download.id}</span>
            <CopyButton text={download.id} label={t("download.copyId")} />
          </dd>
        </div>

        {download.queueId ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.queue")}</dt>
            <dd>{download.queueId}</dd>
          </div>
        ) : null}

        {download.errorCount !== undefined || download.maxRetries !== undefined ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.retries")}</dt>
            <dd>
              {download.errorCount ?? 0}
              {" / "}
              {download.maxRetries != null && download.maxRetries > 0
                ? download.maxRetries
                : t("download.retriesQueueDefault")}
            </dd>
          </div>
        ) : null}

        {download.filenameInferred !== undefined ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.filenameSource")}</dt>
            <dd>
              {download.filenameInferred
                ? t("download.filenameInferred")
                : t("download.filenameUser")}
            </dd>
          </div>
        ) : null}
      </dl>

      <div className="avar-download-detail__actions">
        {download.url ? (
          <Button size="sm" variant="secondary" onClick={() => void handleCopyCurl()}>
            <FontAwesomeIcon icon={faCopy} />
            {copied ? t("download.curlCopied") : t("download.copyCurl")}
          </Button>
        ) : null}
        {onOpenPopup ? (
          <Button size="sm" variant="secondary" onClick={onOpenPopup}>
            {t("download.openWindow")}
          </Button>
        ) : null}
      </div>
    </div>
  );
}
