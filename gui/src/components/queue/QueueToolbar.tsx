import { useRef } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { Input } from "@/components/ui/Input";
import { QueueControls } from "@/components/queue/QueueControls";

export interface QueueToolbarProps {
  searchQuery: string;
  onSearchChange: (query: string) => void;
  selectedQueues: QueueInfo[];
  batchBusy?: boolean;
  onBatchStart: (ids: string[]) => void;
  onBatchStop: (ids: string[]) => void;
  onBatchDelete: (ids: string[]) => void;
}

export function QueueToolbar({
  searchQuery,
  onSearchChange,
  selectedQueues,
  batchBusy = false,
  onBatchStart,
  onBatchStop,
  onBatchDelete,
}: QueueToolbarProps) {
  const { t } = useTranslation();
  const searchRef = useRef<HTMLInputElement>(null);

  return (
    <div className="avar-download-toolbar avar-queue-toolbar">
      <div className="avar-download-toolbar__start">
        {selectedQueues.length > 0 ? (
          <div className="avar-download-toolbar__group">
            <span className="avar-download-toolbar__selection">
              {t("queue.selectedCount", { count: selectedQueues.length })}
            </span>
            <QueueControls
              queues={selectedQueues}
              busy={batchBusy}
              onStart={onBatchStart}
              onStop={onBatchStop}
              onDelete={onBatchDelete}
            />
          </div>
        ) : null}
      </div>

      <Input
        ref={searchRef}
        className="avar-download-toolbar__search avar-download-toolbar__search--compact"
        value={searchQuery}
        onChange={(e) => onSearchChange(e.target.value)}
        placeholder={t("queue.searchPlaceholder")}
        aria-label={t("queue.searchPlaceholder")}
      />
    </div>
  );
}
