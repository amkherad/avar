import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { FontAwesomeIcon } from "@/icons";
import { faPlay, faStop, faTrash } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { TruncateWithTooltip } from "@/components/ui/TruncateWithTooltip";

export interface QueueListProps {
  queues: QueueInfo[];
  selectedId: string | null;
  downloadCounts: Record<string, number>;
  showDelete?: boolean;
  onSelect: (id: string) => void;
  onStart: (id: string) => void;
  onStop: (id: string) => void;
  onDelete: (id: string) => void;
  busyId: string | null;
}

export function QueueList({
  queues,
  selectedId,
  downloadCounts,
  showDelete = false,
  onSelect,
  onStart,
  onStop,
  onDelete,
  busyId,
}: QueueListProps) {
  const { t } = useTranslation();

  if (queues.length === 0) {
    return <p className="avar-empty">{t("queue.empty")}</p>;
  }

  return (
    <ul className="avar-list">
      {queues.map((queue) => {
        const busy = busyId === queue.id;
        const meta = queue.description ?? queue.id;
        return (
          <li key={queue.id}>
            <div
              className={`avar-list__item ${selectedId === queue.id ? "avar-list__item--active" : ""}`}
              style={{ cursor: "default" }}
            >
              <button
                type="button"
                className="avar-list__item-select"
                onClick={() => onSelect(queue.id)}
              >
                <TruncateWithTooltip text={queue.name} className="avar-list__title" />
                <TruncateWithTooltip text={meta} className="avar-list__meta" />
                <span style={{ display: "flex", gap: "0.35rem", marginTop: "0.25rem" }}>
                  {!queue.readonly ? (
                    <Badge tone={queue.running ? "success" : "default"}>
                      {queue.running ? t("queue.running") : t("queue.stopped")}
                    </Badge>
                  ) : null}
                  <Badge tone="info">
                    {t("queue.downloads", { count: downloadCounts[queue.id] ?? 0 })}
                  </Badge>
                </span>
              </button>
              {!queue.readonly ? (
                <div className="avar-queue-actions">
                  {queue.running ? (
                    <Button
                      size="sm"
                      variant="secondary"
                      loading={busy}
                      onClick={() => onStop(queue.id)}
                    >
                      <FontAwesomeIcon icon={faStop} />
                      {t("queue.stop")}
                    </Button>
                  ) : (
                    <Button
                      size="sm"
                      variant="secondary"
                      loading={busy}
                      onClick={() => onStart(queue.id)}
                    >
                      <FontAwesomeIcon icon={faPlay} />
                      {t("queue.start")}
                    </Button>
                  )}
                  {showDelete ? (
                    <Button
                      size="sm"
                      variant="ghost"
                      loading={busy}
                      onClick={() => onDelete(queue.id)}
                    >
                      <FontAwesomeIcon icon={faTrash} />
                      {t("queue.delete")}
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
