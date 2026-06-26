import { useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faCopy, faDownload, faFolder, faFolderOpen, faRotateRight } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { CopyButton } from "@/components/ui/CopyButton";
import type { DownloadInfo } from "@/api/types";
import { formatDownloadStatus } from "@/lib/downloadStatusLabel";
import { canRedownload, isCompleted } from "@/lib/downloadStatus";
import { buildDownloadCurl, copyTextToClipboard } from "@/lib/curlCommand";
import { useDownloadActions } from "@/hooks/useDownloadActions";
import { useDownloadProgressWatch } from "@/hooks/useDownloadProgressWatch";
import { useUnitFormat } from "@/hooks/useUnitFormat";
import { progressPercent } from "./format";
import { DownloadProgressBar } from "./DownloadProgressBar";
import { DownloadChecksumTools } from "./DownloadChecksumTools";

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
  useDownloadProgressWatch(download.id);
  const { busy, redownload, copyToLocal, copyToLocalAvailable, copyToLocalVisible, localCopyReady, openFile, openContainingFolder, openFileVisible } =
    useDownloadActions();
  const { formatBytePair, formatTransferRate } = useUnitFormat();
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
          <dd className="avar-download-detail__progress">
            <DownloadProgressBar download={download} />
            <span className="avar-list__meta">
              {formatBytePair(download.bytesDownloaded, download.totalBytes)} ({percent}%)
            </span>
          </dd>
        </div>

        {download.bytesPerSecond !== undefined && download.bytesPerSecond > 0 ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.transferRate")}</dt>
            <dd>{formatTransferRate(download.bytesPerSecond)}</dd>
          </div>
        ) : null}

        {download.url ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.url")}</dt>
            <dd className="avar-download-detail__value-row">
              <span className="avar-download-detail__url">{download.url}</span>
              <CopyButton text={download.url} label={t("download.copyUrl")} />
            </dd>
          </div>
        ) : null}

        {download.referer ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.referer")}</dt>
            <dd className="avar-download-detail__value-row">
              <span className="avar-download-detail__url">{download.referer}</span>
              <CopyButton text={download.referer} label={t("download.copyReferer")} />
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

        {download.status === "error" && download.description ? (
          <div className="avar-download-detail__field">
            <dt>{t("download.errorMessage")}</dt>
            <dd className="avar-download-detail__error">{download.description}</dd>
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
        {canRedownload(download.status) && download.url ? (
          <Button
            size="sm"
            variant="secondary"
            loading={busy}
            onClick={() => void redownload([download])}
          >
            <FontAwesomeIcon icon={faRotateRight} />
            {t("download.redownload")}
          </Button>
        ) : null}
        {copyToLocalVisible && isCompleted(download.status) ? (
          <Button
            size="sm"
            variant="secondary"
            loading={busy}
            disabled={!copyToLocalAvailable}
            title={!localCopyReady ? t("download.copyToLocalNeedsPath") : undefined}
            onClick={() => void copyToLocal([download])}
          >
            <FontAwesomeIcon icon={faDownload} />
            {t("download.copyToLocal")}
          </Button>
        ) : null}
        {openFileVisible && isCompleted(download.status) ? (
          <Button
            size="sm"
            variant="secondary"
            loading={busy}
            onClick={() => void openFile([download])}
          >
            <FontAwesomeIcon icon={faFolderOpen} />
            {t("download.openFile")}
          </Button>
        ) : null}
        {openFileVisible && isCompleted(download.status) ? (
          <Button
            size="sm"
            variant="secondary"
            loading={busy}
            onClick={() => void openContainingFolder([download])}
          >
            <FontAwesomeIcon icon={faFolder} />
            {t("download.openContainingFolder")}
          </Button>
        ) : null}
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

      <DownloadChecksumTools downloadId={download.id} status={download.status} />
    </div>
  );
}
