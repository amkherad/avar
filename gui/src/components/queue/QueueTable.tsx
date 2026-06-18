import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { FontAwesomeIcon } from "@/icons";
import { faPenToSquare, faPlay, faStop, faTrash } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { Spinner } from "@/components/ui/Spinner";
import { TruncateWithTooltip } from "@/components/ui/TruncateWithTooltip";
import { QueueContextMenu } from "@/components/queue/QueueContextMenu";

export interface QueueTableProps {
  queues: QueueInfo[];
  downloadCounts: Record<string, number>;
  showDelete?: boolean;
  showModify?: boolean;
  onStart: (id: string) => void;
  onStop: (id: string) => void;
  onDelete: (id: string) => void;
  onModify?: (id: string) => void;
  busyId: string | null;
  loading?: boolean;
}

export function QueueTable({
  queues,
  downloadCounts,
  showDelete = false,
  showModify = false,
  onStart,
  onStop,
  onDelete,
  onModify,
  busyId,
  loading = false,
}: QueueTableProps) {
  const { t } = useTranslation();
  const [contextMenu, setContextMenu] = useState<{
    queue: QueueInfo;
    x: number;
    y: number;
  } | null>(null);

  if (loading) {
    return (
      <div className="avar-queue-table__empty">
        <Spinner />
      </div>
    );
  }

  if (queues.length === 0) {
    return <p className="avar-empty">{t("queue.empty")}</p>;
  }

  return (
    <>
    <div className="avar-queue-table">
      <div className="avar-queue-table__header" role="row">
        <div className="avar-queue-table__th" role="columnheader">
          {t("queue.name")}
        </div>
        <div className="avar-queue-table__th" role="columnheader">
          {t("queue.tableDescription")}
        </div>
        <div className="avar-queue-table__th" role="columnheader">
          {t("queue.tableStatus")}
        </div>
        <div className="avar-queue-table__th" role="columnheader">
          {t("queue.tableDownloads")}
        </div>
        <div className="avar-queue-table__th avar-queue-table__th--actions" role="columnheader" />
      </div>

      <div className="avar-queue-table__body" role="rowgroup">
        {queues.map((queue) => {
          const busy = busyId === queue.id;
          const count = downloadCounts[queue.id] ?? 0;
          const statusLabel = queue.running ? t("queue.running") : t("queue.stopped");

          return (
            <div
              key={queue.id}
              className="avar-queue-table__row"
              role="row"
              onContextMenu={(event) => {
                if (queue.readonly) {
                  return;
                }
                event.preventDefault();
                setContextMenu({
                  queue,
                  x: event.clientX,
                  y: event.clientY,
                });
              }}
            >
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
              <div className="avar-queue-table__cell avar-queue-table__cell--actions" role="cell">
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
        })}
      </div>
    </div>

    <QueueContextMenu
      queue={contextMenu?.queue ?? null}
      position={contextMenu ? { x: contextMenu.x, y: contextMenu.y } : null}
      showDelete={showDelete}
      showModify={showModify}
      busy={contextMenu ? busyId === contextMenu.queue.id : false}
      onClose={() => setContextMenu(null)}
      onStart={onStart}
      onStop={onStop}
      onDelete={onDelete}
      onModify={onModify}
    />
    </>
  );
}
