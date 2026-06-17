import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { FontAwesomeIcon } from "@/icons";
import { faPlay, faStop, faTrash } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";

export interface QueueListProps {
  queues: QueueInfo[];
  selectedId: string | null;
  downloadCounts: Record<string, number>;
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
                <span className="avar-list__title">{queue.name}</span>
                <span className="avar-list__meta">
                  {queue.description ?? queue.id}
                </span>
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
                <Button
                  size="sm"
                  variant="ghost"
                  loading={busy}
                  onClick={() => onDelete(queue.id)}
                >
                  <FontAwesomeIcon icon={faTrash} />
                  {t("queue.delete")}
                </Button>
              </div>
              ) : null}
            </div>
          </li>
        );
      })}
    </ul>
  );
}
