import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import { faTableCells, faTableList } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { DataTable, type DataTableColumn } from "@/components/ui/DataTable";
import { Select } from "@/components/ui/Select";
import { TruncateWithTooltip } from "@/components/ui/TruncateWithTooltip";
import type { DownloadInfo } from "@/api/types";
import { useLayoutStore, type DownloadViewMode } from "@/stores/layoutStore";
import { progressPercent } from "./format";
import { formatDownloadStatus } from "@/lib/downloadStatusLabel";
import { useUnitFormat } from "@/hooks/useUnitFormat";
import type { DownloadSort, DownloadStatusFilter } from "@/lib/downloadFilterSort";

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
  statusFilter: DownloadStatusFilter;
  onStatusFilterChange: (filter: DownloadStatusFilter) => void;
  availableStatuses: string[];
  sort: DownloadSort;
  onSortChange: (sort: DownloadSort) => void;
  onPageChange: (page: number) => void;
  onPageSizeChange: (size: number) => void;
  onSelect: (id: string, event?: React.MouseEvent) => void;
  onToggleSelect: (id: string) => void;
  onSelectAll: (checked: boolean) => void;
  onOpen: (id: string) => void;
  onContextMenu?: (id: string, event: React.MouseEvent) => void;
  viewMode: DownloadViewMode;
  onViewModeChange: (mode: DownloadViewMode) => void;
  onToggleCheckboxes: () => void;
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

export function DownloadTable({
  downloads,
  selectedIds,
  loading = false,
  emptyMessage,
  showCheckboxes = false,
  page,
  pageSize,
  totalItems,
  statusFilter,
  onStatusFilterChange,
  availableStatuses,
  sort,
  onSortChange,
  onPageChange,
  onPageSizeChange,
  onSelect,
  onToggleSelect,
  onSelectAll,
  onOpen,
  onContextMenu,
  viewMode,
  onViewModeChange,
  onToggleCheckboxes,
}: DownloadTableProps) {
  const { t } = useTranslation();
  const { formatBytePair, formatTransferRate } = useUnitFormat();
  const columnWidths = useLayoutStore((s) => s.downloadTableColumns);
  const setColumn = useLayoutStore((s) => s.setDownloadTableColumn);
  const totalPages = Math.max(1, Math.ceil(totalItems / pageSize));

  const columns = useMemo((): DataTableColumn<DownloadInfo>[] => {
    return [
      {
        id: "filename",
        header: t("download.filename"),
        width: columnWidths.filename,
        minWidth: 60,
        maxWidth: 800,
        onResize: (width) => setColumn("filename", width),
        render: (item) => (
          <TruncateWithTooltip text={item.filename} className="avar-list__title" />
        ),
      },
      {
        id: "status",
        header: t("download.status"),
        sortKey: "status",
        width: columnWidths.status,
        minWidth: 60,
        maxWidth: 400,
        onResize: (width) => setColumn("status", width),
        headerLayout: "stacked",
        headerAddon: (
          <Select
            compact
            className="avar-data-table__header-filter"
            value={statusFilter}
            aria-label={t("download.statusFilter")}
            onClick={(e) => e.stopPropagation()}
            onChange={(e) => onStatusFilterChange(e.target.value as DownloadStatusFilter)}
          >
            <option value="all">{t("download.statusFilterAll")}</option>
            {availableStatuses.map((status) => (
              <option key={status} value={status}>
                {formatDownloadStatus(status, t)}
              </option>
            ))}
          </Select>
        ),
        render: (item) => (
          <Badge tone={statusTone(item.status)}>
            {formatDownloadStatus(item.status, t)}
          </Badge>
        ),
      },
      {
        id: "progress",
        header: t("download.progress"),
        sortKey: "progress",
        width: columnWidths.progress,
        minWidth: 60,
        maxWidth: 400,
        onResize: (width) => setColumn("progress", width),
        render: (item) => {
          const percent = progressPercent(item.bytesDownloaded, item.totalBytes);
          const progressText = `${formatBytePair(item.bytesDownloaded, item.totalBytes)} (${percent}%)`;
          return <TruncateWithTooltip text={progressText} className="avar-list__meta" />;
        },
      },
      {
        id: "speed",
        header: t("download.transferRate"),
        width: columnWidths.speed,
        minWidth: 60,
        maxWidth: 200,
        onResize: (width) => setColumn("speed", width),
        render: (item) => (
          <TruncateWithTooltip
            text={formatTransferRate(item.bytesPerSecond ?? 0)}
            className="avar-list__meta"
          />
        ),
      },
    ];
  }, [
    availableStatuses,
    columnWidths,
    formatBytePair,
    formatTransferRate,
    onStatusFilterChange,
    setColumn,
    statusFilter,
    t,
  ]);

  return (
    <DataTable
      rows={downloads}
      columns={columns}
      getRowId={(item) => item.id || item.filename}
      selectedIds={selectedIds}
      showCheckboxes={showCheckboxes}
      selectAllLabel={t("download.selectAll")}
      getCheckboxLabel={(item) => item.filename}
      onToggleSelect={onToggleSelect}
      onSelectAll={onSelectAll}
      sort={sort}
      onSortChange={(next) =>
        onSortChange({
          key: next.key as DownloadSort["key"],
          direction: next.direction,
        })
      }
      loading={loading}
      emptyMessage={emptyMessage ?? t("download.empty")}
      trailing={{ width: 48, variant: "fill" }}
      onRowClick={(item, event) => onSelect(item.id, event)}
      onRowDoubleClick={(item) => onOpen(item.id)}
      onRowContextMenu={(item, event) => onContextMenu?.(item.id, event)}
      variant="flex"
      interactive
      chrome={{
        viewModeLabel: t("download.viewMode"),
        viewButtons: [
          {
            id: "grid",
            label: t("download.viewGrid"),
            icon: faTableCells,
            active: viewMode === "grid",
            onClick: () => onViewModeChange("grid"),
          },
          {
            id: "compact",
            label: t("download.viewCompact"),
            icon: faTableList,
            active: viewMode === "compact",
            onClick: () => onViewModeChange("compact"),
          },
        ],
        showCheckboxes,
        onToggleCheckboxes,
        toggleCheckboxesLabel: t("download.toggleCheckboxes"),
        settingsLabel: t("table.settings"),
      }}
      pagination={{
        page,
        pageSize,
        totalItems,
        pageSizes: DOWNLOAD_PAGE_SIZES,
        pageSizeLabel: t("download.pageSize"),
        prevLabel: t("download.prevPage"),
        nextLabel: t("download.nextPage"),
        pageLabel: t("download.pageLabel", { page, total: totalPages }),
        onPageChange,
        onPageSizeChange,
      }}
    />
  );
}
