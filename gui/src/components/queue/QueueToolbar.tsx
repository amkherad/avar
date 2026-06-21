import { useRef } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faCheckSquare } from "@fortawesome/free-solid-svg-icons";
import type { QueueInfo } from "@/api/types";
import { Input } from "@/components/ui/Input";
import { Button } from "@/components/ui/Button";
import { QueueControls } from "@/components/queue/QueueControls";

export interface QueueToolbarProps {
  searchQuery: string;
  onSearchChange: (query: string) => void;
  selectedQueues: QueueInfo[];
  showCheckboxes: boolean;
  onToggleCheckboxes: () => void;
  batchBusy?: boolean;
  onBatchStart: (ids: string[]) => void;
  onBatchStop: (ids: string[]) => void;
  onBatchDelete: (ids: string[]) => void;
}

export function QueueToolbar({
  searchQuery,
  onSearchChange,
  selectedQueues,
  showCheckboxes,
  onToggleCheckboxes,
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

        <div className="avar-download-toolbar__group">
          <Button
            size="sm"
            variant={showCheckboxes ? "secondary" : "ghost"}
            aria-pressed={showCheckboxes}
            title={t("queue.toggleCheckboxes")}
            onClick={onToggleCheckboxes}
          >
            <FontAwesomeIcon icon={faCheckSquare} />
          </Button>
        </div>
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
