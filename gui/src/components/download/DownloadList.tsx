import { formatBytePair, progressPercent } from "./format";
import { Badge } from "@/components/ui/Badge";
import type { DownloadInfo } from "@/api/types";

export interface DownloadProgressProps {
  download: DownloadInfo;
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

export function DownloadProgress({ download }: DownloadProgressProps) {
  const percent = progressPercent(download.bytesDownloaded, download.totalBytes);

  return (
    <div>
      <div className="avar-progress" aria-hidden="true">
        <div className="avar-progress__bar" style={{ width: `${percent}%` }} />
      </div>
      <span className="avar-list__meta">
        {formatBytePair(download.bytesDownloaded, download.totalBytes)} ({percent}%)
      </span>
    </div>
  );
}

export function DownloadRow({ download }: DownloadProgressProps) {
  return (
    <div className="avar-download-row">
      <div>
        <div className="avar-list__title">{download.filename}</div>
        {download.url ? <div className="avar-list__meta">{download.url}</div> : null}
        <DownloadProgress download={download} />
      </div>
      <Badge tone={statusTone(download.status)}>{download.status}</Badge>
    </div>
  );
}
