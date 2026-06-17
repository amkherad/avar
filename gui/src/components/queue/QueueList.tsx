import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { FontAwesomeIcon } from "@/icons";
import { faPenToSquare, faPlay, faStop, faTrash } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { TruncateWithTooltip } from "@/components/ui/TruncateWithTooltip";

export interface QueueListProps {
  queues: QueueInfo[];
  selectedId: string | null;
  downloadCounts: Record<string, number>;
  showDelete?: boolean;
  selectable?: boolean;
  showModify?: boolean;
  onSelect: (id: string) => void;
  onStart: (id: string) => void;
  onStop: (id: string) => void;
  onDelete: (id: string) => void;
  onModify?: (id: string) => void;
  busyId: string | null;
}

export function QueueList({
  queues,
  selectedId,
  downloadCounts,
  showDelete = false,
  selectable = true,
  showModify = false,
  onSelect,
  onStart,
  onStop,
  onDelete,
  onModify,
  busyId,
}: QueueListProps) {
  const { t } = useTranslation();

  if (queues.length === 0) {
    return <p className="avar-empty">{t("queue.empty")}</p>;
  }

  return (
    <ul className="avar-list avar-striped-list avar-queue-list--compact">
      {queues.map((queue) => {
        const busy = busyId === queue.id;
        const isActive = selectable && selectedId === queue.id;
        const count = downloadCounts[queue.id] ?? 0;
        const statusLabel = queue.running ? t("queue.running") : t("queue.stopped");

        return (
          <li key={queue.id} className="avar-queue-list__item">
            <div
              className={`avar-list__item avar-queue-list__row ${isActive ? "avar-list__item--active" : ""}`}
              style={{ cursor: selectable ? "pointer" : "default" }}
            >
              <button
                type="button"
                className="avar-list__item-select avar-queue-list__select"
                disabled={!selectable}
                onClick={() => {
                  if (selectable) {
                    onSelect(queue.id);
                  }
                }}
              >
                <TruncateWithTooltip text={queue.name} className="avar-list__title" />
              </button>

              <div className="avar-queue-list__hover" role="tooltip">
                <p className="avar-queue-list__hover-title">{queue.name}</p>
                {queue.description ? (
                  <p className="avar-queue-list__hover-meta">{queue.description}</p>
                ) : null}
                <div className="avar-queue-list__hover-badges">
                  {!queue.readonly ? (
                    <Badge tone={queue.running ? "success" : "default"}>{statusLabel}</Badge>
                  ) : null}
                  <Badge tone="info">{t("queue.downloads", { count })}</Badge>
                </div>
              </div>

              {!queue.readonly ? (
                <div className="avar-queue-actions avar-queue-actions--compact">
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
          </li>
        );
      })}
    </ul>
  );
}
