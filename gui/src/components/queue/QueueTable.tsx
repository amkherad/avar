import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { FontAwesomeIcon } from "@/icons";
import { faPenToSquare, faPlay, faStop, faTrash } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import { Select } from "@/components/ui/Select";
import { Spinner } from "@/components/ui/Spinner";
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
  onContextMenu,
  busyId,
  batchBusy = false,
}: QueueTableProps) {
  const { t } = useTranslation();
  const columns = useLayoutStore((s) => s.queueTableColumns);
  const setColumn = useLayoutStore((s) => s.setQueueTableColumn);
  const [contextMenu, setContextMenu] = useState<{
    queue: QueueInfo;
    x: number;
    y: number;
  } | null>(null);

  const checkboxCol = showCheckboxes ? "2.25rem " : "";
  const gridTemplate = `${checkboxCol}${columns.name}px ${columns.description}px ${columns.status}px ${columns.downloads}px 3rem`;
  const tableMinWidth =
    (showCheckboxes ? 36 : 0) +
    columns.name +
    columns.description +
    columns.status +
    columns.downloads +
    48;
  const allSelected = queues.length > 0 && queues.every((queue) => selectedIds.includes(queue.id));

  function toggleSort(key: NonNullable<QueueSort["key"]>) {
    if (sort.key !== key) {
      onSortChange({ key, direction: "asc" });
      return;
    }
    if (sort.direction === "asc") {
      onSortChange({ key, direction: "desc" });
      return;
    }
    onSortChange({ key: null, direction: "asc" });
  }

  function sortIndicator(key: NonNullable<QueueSort["key"]>): string {
    if (sort.key !== key) {
      return "";
    }
    return sort.direction === "asc" ? " ▲" : " ▼";
  }

  function openContextMenu(queue: QueueInfo, event: React.MouseEvent) {
    if (queue.readonly) {
      return;
    }
    event.preventDefault();
    onContextMenu?.(queue.id, event);
    setContextMenu({
      queue,
      x: event.clientX,
      y: event.clientY,
    });
  }

  return (
    <>
      <div className="avar-queue-table">
        <div className="avar-queue-table__scroll">
          <div className="avar-queue-table__inner" style={{ minWidth: tableMinWidth }}>
            <div
              className="avar-queue-table__header"
              style={{ gridTemplateColumns: gridTemplate }}
              role="row"
            >
              {showCheckboxes ? (
                <div
                  className="avar-queue-table__th avar-queue-table__th--checkbox"
                  role="columnheader"
                >
                  <input
                    type="checkbox"
                    aria-label={t("queue.selectAll")}
                    checked={allSelected}
                    onChange={(e) => onSelectAll(e.target.checked)}
                  />
                </div>
              ) : null}
              <div className="avar-queue-table__th avar-queue-table__th--sortable" role="columnheader">
                <button
                  type="button"
                  className="avar-queue-table__sort-btn"
                  onClick={() => toggleSort("name")}
                >
                  {t("queue.name")}
                  {sortIndicator("name")}
                </button>
                <ResizeHandle
                  axis="horizontal"
                  min={60}
                  max={400}
                  className="avar-queue-table__col-resize"
                  onResize={(delta) => setColumn("name", columns.name + delta)}
                />
              </div>
              <div className="avar-queue-table__th avar-queue-table__th--sortable" role="columnheader">
                <button
                  type="button"
                  className="avar-queue-table__sort-btn"
                  onClick={() => toggleSort("description")}
                >
                  {t("queue.tableDescription")}
                  {sortIndicator("description")}
                </button>
                <ResizeHandle
                  axis="horizontal"
                  min={60}
                  max={600}
                  className="avar-queue-table__col-resize"
                  onResize={(delta) => setColumn("description", columns.description + delta)}
                />
              </div>
              <div className="avar-queue-table__th avar-queue-table__th--sortable" role="columnheader">
                <div className="avar-queue-table__th-main">
                  <button
                    type="button"
                    className="avar-queue-table__sort-btn"
                    onClick={() => toggleSort("status")}
                  >
                    {t("queue.tableStatus")}
                    {sortIndicator("status")}
                  </button>
                  <Select
                    compact
                    className="avar-queue-table__header-filter"
                    value={statusFilter}
                    aria-label={t("queue.statusFilter")}
                    onClick={(e) => e.stopPropagation()}
                    onChange={(e) => onStatusFilterChange(e.target.value as QueueStatusFilter)}
                  >
                    <option value="all">{t("queue.statusFilterAll")}</option>
                    <option value="running">{t("queue.running")}</option>
                    <option value="stopped">{t("queue.stopped")}</option>
                  </Select>
                </div>
                <ResizeHandle
                  axis="horizontal"
                  min={80}
                  max={300}
                  className="avar-queue-table__col-resize"
                  onResize={(delta) => setColumn("status", columns.status + delta)}
                />
              </div>
              <div className="avar-queue-table__th avar-queue-table__th--sortable" role="columnheader">
                <button
                  type="button"
                  className="avar-queue-table__sort-btn"
                  onClick={() => toggleSort("downloads")}
                >
                  {t("queue.tableDownloads")}
                  {sortIndicator("downloads")}
                </button>
                <ResizeHandle
                  axis="horizontal"
                  min={60}
                  max={200}
                  className="avar-queue-table__col-resize"
                  onResize={(delta) => setColumn("downloads", columns.downloads + delta)}
                />
              </div>
              <div
                className="avar-queue-table__th avar-queue-table__th--actions"
                role="columnheader"
              />
            </div>

            <div className="avar-queue-table__body" role="rowgroup">
              {loading ? (
                <div className="avar-queue-table__empty">
                  <Spinner />
                </div>
              ) : queues.length === 0 ? (
                <div className="avar-queue-table__empty">
                  <p className="avar-empty">{emptyMessage ?? t("queue.empty")}</p>
                </div>
              ) : (
                queues.map((queue) => {
                  const busy = busyId === queue.id || batchBusy;
                  const count = downloadCounts[queue.id] ?? 0;
                  const statusLabel = queue.running ? t("queue.running") : t("queue.stopped");
                  const selected = selectedIds.includes(queue.id);

                  return (
                    <div
                      key={queue.id}
                      className={`avar-queue-table__row ${selected ? "avar-queue-table__row--selected" : ""}`}
                      style={{ gridTemplateColumns: gridTemplate }}
                      role="row"
                      onContextMenu={(event) => openContextMenu(queue, event)}
                    >
                      {showCheckboxes ? (
                        <div
                          className="avar-queue-table__cell avar-queue-table__cell--checkbox"
                          role="cell"
                          onClick={(event) => event.stopPropagation()}
                        >
                          <input
                            type="checkbox"
                            aria-label={queue.name}
                            checked={selected}
                            disabled={queue.readonly}
                            onChange={() => onToggleSelect(queue.id)}
                          />
                        </div>
                      ) : null}
                      <div className="avar-queue-table__cell" role="cell">
                        <TruncateWithTooltip text={queue.name} className="avar-list__title" />
                      </div>
                      <div className="avar-queue-table__cell" role="cell">
                        <TruncateWithTooltip
                          text={queue.description || "—"}
                          className="avar-list__meta"
                        />
                      </div>
                      <div className="avar-queue-table__cell" role="cell">
                        {queue.readonly ? (
                          <span className="avar-list__meta">—</span>
                        ) : (
                          <Badge tone={queue.running ? "success" : "default"}>{statusLabel}</Badge>
                        )}
                      </div>
                      <div className="avar-queue-table__cell" role="cell">
                        <Badge tone="info">{t("queue.downloads", { count })}</Badge>
                      </div>
                      <div
                        className="avar-queue-table__cell avar-queue-table__cell--actions"
                        role="cell"
                      >
                        {!queue.readonly ? (
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
                        ) : null}
                      </div>
                    </div>
                  );
                })
              )}
            </div>
          </div>
        </div>
      </div>

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
