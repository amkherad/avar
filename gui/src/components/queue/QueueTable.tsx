import { useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { FontAwesomeIcon } from "@/icons";
import { faPenToSquare, faPlay, faStop, faTrash } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { DataTable, type DataTableColumn } from "@/components/ui/DataTable";
import { Select } from "@/components/ui/Select";
import { TruncateWithTooltip } from "@/components/ui/TruncateWithTooltip";
import { QueueContextMenu } from "@/components/queue/QueueContextMenu";
import { useLayoutStore } from "@/stores/layoutStore";
import type { QueueSort, QueueStatusFilter } from "@/lib/queueFilterSort";

export interface QueueTableProps {
  queues: QueueInfo[];
  downloadCounts: Record<string, number>;
  selectedIds: string[];
  showCheckboxes?: boolean;
  showDelete?: boolean;
  showModify?: boolean;
  loading?: boolean;
  emptyMessage?: string;
  statusFilter: QueueStatusFilter;
  onStatusFilterChange: (filter: QueueStatusFilter) => void;
  sort: QueueSort;
  onSortChange: (sort: QueueSort) => void;
  onStart: (id: string) => void;
  onStop: (id: string) => void;
  onDelete: (id: string) => void;
  onModify?: (id: string) => void;
  onToggleSelect: (id: string) => void;
  onSelectAll: (checked: boolean) => void;
  onToggleCheckboxes: () => void;
  onContextMenu?: (id: string, event: React.MouseEvent) => void;
  busyId: string | null;
  batchBusy?: boolean;
}

export function QueueTable({
  queues,
  downloadCounts,
  selectedIds,
  showCheckboxes = false,
  showDelete = false,
  showModify = false,
  loading = false,
  emptyMessage,
  statusFilter,
  onStatusFilterChange,
  sort,
  onSortChange,
  onStart,
  onStop,
  onDelete,
  onModify,
  onToggleSelect,
  onSelectAll,
  onToggleCheckboxes,
  onContextMenu,
  busyId,
  batchBusy = false,
}: QueueTableProps) {
  const { t } = useTranslation();
  const columnWidths = useLayoutStore((s) => s.queueTableColumns);
  const setColumn = useLayoutStore((s) => s.setQueueTableColumn);
  const [contextMenu, setContextMenu] = useState<{
    queue: QueueInfo;
    x: number;
    y: number;
  } | null>(null);

  const columns = useMemo((): DataTableColumn<QueueInfo>[] => {
    return [
      {
        id: "name",
        header: t("queue.name"),
        sortKey: "name",
        width: columnWidths.name,
        minWidth: 60,
        maxWidth: 400,
        onResize: (width) => setColumn("name", width),
        render: (queue) => (
          <TruncateWithTooltip text={queue.name} className="avar-list__title" />
        ),
      },
      {
        id: "description",
        header: t("queue.tableDescription"),
        sortKey: "description",
        width: columnWidths.description,
        minWidth: 60,
        maxWidth: 600,
        onResize: (width) => setColumn("description", width),
        render: (queue) => (
          <TruncateWithTooltip text={queue.description || "—"} className="avar-list__meta" />
        ),
      },
      {
        id: "status",
        header: t("queue.tableStatus"),
        sortKey: "status",
        headerLayout: "stacked",
        width: columnWidths.status,
        minWidth: 80,
        maxWidth: 300,
        onResize: (width) => setColumn("status", width),
        headerAddon: (
          <Select
            compact
            className="avar-data-table__header-filter"
            value={statusFilter}
            aria-label={t("queue.statusFilter")}
            onClick={(e) => e.stopPropagation()}
            onChange={(e) => onStatusFilterChange(e.target.value as QueueStatusFilter)}
          >
            <option value="all">{t("queue.statusFilterAll")}</option>
            <option value="running">{t("queue.running")}</option>
            <option value="stopped">{t("queue.stopped")}</option>
          </Select>
        ),
        render: (queue) => {
          if (queue.readonly) {
            return <span className="avar-list__meta">—</span>;
          }
          const statusLabel = queue.running ? t("queue.running") : t("queue.stopped");
          return <Badge tone={queue.running ? "success" : "default"}>{statusLabel}</Badge>;
        },
      },
      {
        id: "downloads",
        header: t("queue.tableDownloads"),
        sortKey: "downloads",
        width: columnWidths.downloads,
        minWidth: 60,
        maxWidth: 200,
        onResize: (width) => setColumn("downloads", width),
        render: (queue) => {
          const count = downloadCounts[queue.id] ?? 0;
          return <Badge tone="info">{t("queue.downloads", { count })}</Badge>;
        },
      },
    ];
  }, [columnWidths, downloadCounts, onStatusFilterChange, setColumn, statusFilter, t]);

  function renderActions(queue: QueueInfo) {
    if (queue.readonly) {
      return null;
    }
    const busy = busyId === queue.id || batchBusy;
    return (
      <div className="avar-queue-actions avar-queue-actions--table">
        {queue.running ? (
          <Button
            size="sm"
            variant="secondary"
            className="avar-btn--icon-only"
            loading={busy}
            aria-label={t("queue.stop")}
            title={t("queue.stop")}
            onClick={() => onStop(queue.id)}
          >
            <FontAwesomeIcon icon={faStop} />
          </Button>
        ) : (
          <Button
            size="sm"
            variant="secondary"
            className="avar-btn--icon-only"
            loading={busy}
            aria-label={t("queue.start")}
            title={t("queue.start")}
            onClick={() => onStart(queue.id)}
          >
            <FontAwesomeIcon icon={faPlay} />
          </Button>
        )}
        {showModify && onModify ? (
          <Button
            size="sm"
            variant="ghost"
            className="avar-btn--icon-only"
            aria-label={t("queue.modify")}
            title={t("queue.modify")}
            onClick={() => onModify(queue.id)}
          >
            <FontAwesomeIcon icon={faPenToSquare} />
          </Button>
        ) : null}
        {showDelete ? (
          <Button
            size="sm"
            variant="ghost"
            className="avar-btn--icon-only"
            loading={busy}
            aria-label={t("queue.delete")}
            title={t("queue.delete")}
            onClick={() => onDelete(queue.id)}
          >
            <FontAwesomeIcon icon={faTrash} />
          </Button>
        ) : null}
      </div>
    );
  }

  return (
    <>
      <DataTable
        rows={queues}
        columns={columns}
        getRowId={(queue) => queue.id}
        selectedIds={selectedIds}
        showCheckboxes={showCheckboxes}
        selectAllLabel={t("queue.selectAll")}
        isRowSelectable={(queue) => !queue.readonly}
        getCheckboxLabel={(queue) => queue.name}
        onToggleSelect={onToggleSelect}
        onSelectAll={onSelectAll}
        sort={sort}
        onSortChange={(next) =>
          onSortChange({
            key: next.key as QueueSort["key"],
            direction: next.direction,
          })
        }
        loading={loading}
        emptyMessage={emptyMessage ?? t("queue.empty")}
        trailing={{ width: 48, variant: "actions", render: renderActions }}
        onRowContextMenu={(queue, event) => {
          if (queue.readonly) {
            return;
          }
          onContextMenu?.(queue.id, event);
          setContextMenu({
            queue,
            x: event.clientX,
            y: event.clientY,
          });
        }}
        variant="bordered"
        chrome={{
          showCheckboxes,
          onToggleCheckboxes,
          toggleCheckboxesLabel: t("queue.toggleCheckboxes"),
          settingsLabel: t("table.settings"),
        }}
      />

      <QueueContextMenu
        queue={contextMenu?.queue ?? null}
        position={contextMenu ? { x: contextMenu.x, y: contextMenu.y } : null}
        showDelete={showDelete}
        showModify={showModify}
        busy={contextMenu ? busyId === contextMenu.queue.id || batchBusy : false}
        onClose={() => setContextMenu(null)}
        onStart={onStart}
        onStop={onStop}
        onDelete={onDelete}
        onModify={onModify}
      />
    </>
  );
}
