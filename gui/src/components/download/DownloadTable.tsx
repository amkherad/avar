import { useTranslation } from "react-i18next";
import { Badge } from "@/components/ui/Badge";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import { Spinner } from "@/components/ui/Spinner";
import { TruncateWithTooltip } from "@/components/ui/TruncateWithTooltip";
import type { DownloadInfo } from "@/api/types";
import { useLayoutStore } from "@/stores/layoutStore";
import { formatBytePair, progressPercent } from "./format";
import { formatDownloadStatus } from "@/lib/downloadStatusLabel";

export const DOWNLOAD_PAGE_SIZES = [20, 50, 100, 500, 1000] as const;

export interface DownloadTableProps {
  downloads: DownloadInfo[];
  selectedIds: string[];
  loading?: boolean;
  emptyMessage?: string;
  showCheckboxes?: boolean;
  page: number;
  pageSize: number;
  totalItems: number;
  onPageChange: (page: number) => void;
  onPageSizeChange: (size: number) => void;
  onSelect: (id: string, event?: React.MouseEvent) => void;
  onToggleSelect: (id: string) => void;
  onSelectAll: (checked: boolean) => void;
  onOpen: (id: string) => void;
  onContextMenu?: (id: string, event: React.MouseEvent) => void;
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
  selectedIds,
  loading = false,
  emptyMessage,
  showCheckboxes = false,
  page,
  pageSize,
  totalItems,
  onPageChange,
  onPageSizeChange,
  onSelect,
  onToggleSelect,
  onSelectAll,
  onOpen,
  onContextMenu,
}: DownloadTableProps) {
  const { t } = useTranslation();
  const columns = useLayoutStore((s) => s.downloadTableColumns);
  const setColumn = useLayoutStore((s) => s.setDownloadTableColumn);

  const checkboxCol = showCheckboxes ? "2.25rem " : "";
  const gridTemplate = `${checkboxCol}${columns.filename}px ${columns.status}px ${columns.progress}px ${columns.url}px 1fr`;
  const totalPages = Math.max(1, Math.ceil(totalItems / pageSize));
  const showPaging = totalPages > 1;
  const allSelected = downloads.length > 0 && downloads.every((item) => selectedIds.includes(item.id));

  return (
    <div className="avar-download-table">
      <div
        className="avar-download-table__header"
        style={{ gridTemplateColumns: gridTemplate }}
        role="row"
      >
        {showCheckboxes ? (
          <div className="avar-download-table__th avar-download-table__th--checkbox" role="columnheader">
            <input
              type="checkbox"
              aria-label={t("download.selectAll")}
              checked={allSelected}
              onChange={(e) => onSelectAll(e.target.checked)}
            />
          </div>
        ) : null}
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
        {loading ? (
          <div className="avar-download-table__empty">
            <Spinner />
          </div>
        ) : downloads.length === 0 ? (
          <div className="avar-download-table__empty">
            <p className="avar-empty">{emptyMessage ?? t("download.empty")}</p>
          </div>
        ) : (
          downloads.map((item) => {
            const percent = progressPercent(item.bytesDownloaded, item.totalBytes);
            const selected = selectedIds.includes(item.id);
            const progressText = `${formatBytePair(item.bytesDownloaded, item.totalBytes)} (${percent}%)`;
            const urlText = item.url ?? "—";

            return (
              <div
                key={item.id || item.filename}
                className={`avar-download-table__row ${selected ? "avar-download-table__row--selected" : ""}`}
                style={{ gridTemplateColumns: gridTemplate }}
                role="row"
                tabIndex={0}
                onClick={(event) => onSelect(item.id, event)}
                onDoubleClick={() => onOpen(item.id)}
                onContextMenu={(event) => {
                  event.preventDefault();
                  onContextMenu?.(item.id, event);
                }}
                onKeyDown={(e) => {
                  if (e.key === "Enter") {
                    onSelect(item.id);
                  }
                }}
              >
                {showCheckboxes ? (
                  <div
                    className="avar-download-table__cell avar-download-table__cell--checkbox"
                    role="cell"
                    onClick={(event) => event.stopPropagation()}
                  >
                    <input
                      type="checkbox"
                      aria-label={item.filename}
                      checked={selected}
                      onChange={() => onToggleSelect(item.id)}
                    />
                  </div>
                ) : null}
                <div className="avar-download-table__cell" role="cell">
                  <TruncateWithTooltip text={item.filename} className="avar-list__title" />
                </div>
                <div className="avar-download-table__cell" role="cell">
                  <Badge tone={statusTone(item.status)}>
                    {formatDownloadStatus(item.status, t)}
                  </Badge>
                </div>
                <div className="avar-download-table__cell" role="cell">
                  <TruncateWithTooltip text={progressText} className="avar-list__meta" />
                </div>
                <div className="avar-download-table__cell" role="cell">
                  <TruncateWithTooltip text={urlText} className="avar-list__meta" />
                </div>
                <div className="avar-download-table__cell avar-download-table__cell--fill" role="cell" />
              </div>
            );
          })
        )}
      </div>

      {showPaging ? (
        <div className="avar-download-table__pager">
          <label className="avar-download-table__page-size">
            <span>{t("download.pageSize")}</span>
            <select
              value={pageSize}
              onChange={(e) => onPageSizeChange(Number(e.target.value))}
            >
              {DOWNLOAD_PAGE_SIZES.map((size) => (
                <option key={size} value={size}>
                  {size}
                </option>
              ))}
            </select>
          </label>
          <div className="avar-download-table__page-nav">
            <button
              type="button"
              className="avar-btn avar-btn--sm avar-btn--ghost"
              disabled={page <= 1}
              onClick={() => onPageChange(page - 1)}
            >
              {t("download.prevPage")}
            </button>
            <span className="avar-download-table__page-label">
              {t("download.pageLabel", { page, total: totalPages })}
            </span>
            <button
              type="button"
              className="avar-btn avar-btn--sm avar-btn--ghost"
              disabled={page >= totalPages}
              onClick={() => onPageChange(page + 1)}
            >
              {t("download.nextPage")}
            </button>
          </div>
        </div>
      ) : null}
    </div>
  );
}
