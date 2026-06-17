import { useTranslation } from "react-i18next";
import { Badge } from "@/components/ui/Badge";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import type { DownloadInfo } from "@/api/types";
import { useLayoutStore } from "@/stores/layoutStore";
import { formatBytePair, progressPercent } from "./format";

export interface DownloadTableProps {
  downloads: DownloadInfo[];
  selectedId: string | null;
  onSelect: (id: string) => void;
  onOpen: (id: string) => void;
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

export function DownloadTable({
  downloads,
  selectedId,
  onSelect,
  onOpen,
}: DownloadTableProps) {
  const { t } = useTranslation();
  const columns = useLayoutStore((s) => s.downloadTableColumns);
  const setColumn = useLayoutStore((s) => s.setDownloadTableColumn);

  const gridTemplate = `${columns.filename}px ${columns.status}px ${columns.progress}px ${columns.url}px 1fr`;

  return (
    <div className="avar-download-table">
      <div
        className="avar-download-table__header"
        style={{ gridTemplateColumns: gridTemplate }}
        role="row"
      >
        <div className="avar-download-table__th" role="columnheader">
          {t("download.filename")}
          <ResizeHandle
            axis="horizontal"
            min={60}
            max={800}
            className="avar-download-table__col-resize"
            onResize={(delta) => setColumn("filename", columns.filename + delta)}
          />
        </div>
        <div className="avar-download-table__th" role="columnheader">
          {t("download.status")}
          <ResizeHandle
            axis="horizontal"
            min={60}
            max={400}
            className="avar-download-table__col-resize"
            onResize={(delta) => setColumn("status", columns.status + delta)}
          />
        </div>
        <div className="avar-download-table__th" role="columnheader">
          {t("download.progress")}
          <ResizeHandle
            axis="horizontal"
            min={60}
            max={400}
            className="avar-download-table__col-resize"
            onResize={(delta) => setColumn("progress", columns.progress + delta)}
          />
        </div>
        <div className="avar-download-table__th" role="columnheader">
          {t("download.url")}
          <ResizeHandle
            axis="horizontal"
            min={60}
            max={800}
            className="avar-download-table__col-resize"
            onResize={(delta) => setColumn("url", columns.url + delta)}
          />
        </div>
        <div className="avar-download-table__th avar-download-table__th--fill" role="columnheader" />
      </div>

      <div className="avar-download-table__body" role="rowgroup">
        {downloads.map((item) => {
          const percent = progressPercent(item.bytesDownloaded, item.totalBytes);
          const selected = item.id === selectedId;
          return (
            <div
              key={item.id || item.filename}
              className={`avar-download-table__row ${selected ? "avar-download-table__row--selected" : ""}`}
              style={{ gridTemplateColumns: gridTemplate }}
              role="row"
              tabIndex={0}
              onClick={() => {
                const wasSelected = selected;
                onSelect(item.id);
                if (!wasSelected) {
                  onOpen(item.id);
                }
              }}
              onKeyDown={(e) => {
                if (e.key === "Enter" && !selected) {
                  onSelect(item.id);
                  onOpen(item.id);
                }
              }}
            >
              <div className="avar-download-table__cell" role="cell">
                <span className="avar-list__title">{item.filename}</span>
              </div>
              <div className="avar-download-table__cell" role="cell">
                <Badge tone={statusTone(item.status)}>{item.status}</Badge>
              </div>
              <div className="avar-download-table__cell" role="cell">
                <span className="avar-list__meta">
                  {formatBytePair(item.bytesDownloaded, item.totalBytes)} ({percent}%)
                </span>
              </div>
              <div className="avar-download-table__cell" role="cell">
                <span className="avar-list__meta">{item.url ?? "—"}</span>
              </div>
              <div className="avar-download-table__cell avar-download-table__cell--fill" role="cell" />
            </div>
          );
        })}
      </div>
    </div>
  );
}
