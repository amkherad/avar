import { formatBytePair, progressPercent } from "./format";

import { Badge } from "@/components/ui/Badge";

import type { DownloadInfo } from "@/api/types";



export interface DownloadProgressProps {

  download: DownloadInfo;

}



export interface DownloadRowProps extends DownloadProgressProps {

  selected?: boolean;

  onSelect?: () => void;

  onOpen?: () => void;

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



export function DownloadRow({

  download,

  selected = false,

  onSelect,

  onOpen,

}: DownloadRowProps) {

  return (

    <div

      className={`avar-download-row ${selected ? "avar-download-row--selected" : ""}`}

      role="button"

      tabIndex={0}

      onClick={() => {
        const wasSelected = selected;
        onSelect?.();
        if (!wasSelected) {
          onOpen?.();
        }
      }}

      onKeyDown={(e) => {

        if (e.key === "Enter") {

          if (!selected) {
            onSelect?.();
            onOpen?.();
          }

        }

      }}

    >

      <div>

        <div className="avar-list__title">{download.filename}</div>

        {download.url ? <div className="avar-list__meta">{download.url}</div> : null}

        <DownloadProgress download={download} />

      </div>

      <Badge tone={statusTone(download.status)}>{download.status}</Badge>

    </div>

  );

}

