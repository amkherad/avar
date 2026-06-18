import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { QueueContextMenu } from "@/components/queue/QueueContextMenu";

const SIDEBAR_DESCRIPTION_MAX = 72;

function truncateDescription(text: string, maxLength: number): string {
  const trimmed = text.trim();
  if (trimmed.length <= maxLength) {
    return trimmed;
  }
  return `${trimmed.slice(0, Math.max(0, maxLength - 1)).trimEnd()}…`;
}

export interface QueueListProps {
  queues: QueueInfo[];
  selectedId: string | null;
  downloadCounts: Record<string, number>;
  compact?: boolean;
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
  compact = false,
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
  const [contextMenu, setContextMenu] = useState<{
    queue: QueueInfo;
    x: number;
    y: number;
  } | null>(null);

  if (queues.length === 0) {
    return <p className="avar-empty">{t("queue.empty")}</p>;
  }

  if (compact) {
    return (
      <>
        <ul className="avar-list avar-striped-list avar-queue-sidebar-list">
          {queues.map((queue) => {
            const isActive = selectable && selectedId === queue.id;
            const description = queue.description
              ? truncateDescription(queue.description, SIDEBAR_DESCRIPTION_MAX)
              : null;

            return (
              <li key={queue.id}>
                <button
                  type="button"
                  className={`avar-queue-sidebar-list__item ${isActive ? "avar-queue-sidebar-list__item--active" : ""}`}
                  onClick={() => {
                    if (selectable) {
                      onSelect(queue.id);
                    }
                  }}
                  onContextMenu={(event) => {
                    event.preventDefault();
                    setContextMenu({
                      queue,
                      x: event.clientX,
                      y: event.clientY,
                    });
                  }}
                >
                  <span className="avar-queue-sidebar-list__title">{queue.name}</span>
                  {description ? (
                    <span className="avar-queue-sidebar-list__description" title={queue.description}>
                      {description}
                    </span>
                  ) : null}
                </button>
              </li>
            );
          })}
        </ul>

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

  return (
    <ul className="avar-list avar-striped-list avar-queue-list--verbose">
      {queues.map((queue) => {
        const isActive = selectable && selectedId === queue.id;
        const count = downloadCounts[queue.id] ?? 0;

        return (
          <li key={queue.id} className="avar-queue-list__item">
            <div
              className={`avar-list__item avar-queue-list__row ${isActive ? "avar-list__item--active" : ""}`}
              style={{ cursor: selectable ? "pointer" : "default" }}
              role={selectable ? "button" : undefined}
              tabIndex={selectable ? 0 : undefined}
              onClick={() => {
                if (selectable) {
                  onSelect(queue.id);
                }
              }}
              onKeyDown={(event) => {
                if (selectable && (event.key === "Enter" || event.key === " ")) {
                  event.preventDefault();
                  onSelect(queue.id);
                }
              }}
            >
              <div className="avar-queue-list__select">
                <span className="avar-list__title">{queue.name}</span>
                {queue.description ? (
                  <p className="avar-list__meta avar-queue-list__description">{queue.description}</p>
                ) : null}
                <p className="avar-list__meta">{t("queue.downloads", { count })}</p>
              </div>
            </div>
          </li>
        );
      })}
    </ul>
  );
}
