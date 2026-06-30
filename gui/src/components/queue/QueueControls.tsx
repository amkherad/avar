import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faPlay, faStop, faTrash } from "@fortawesome/free-solid-svg-icons";
import type { QueueInfo } from "@/api/types";
import { Button } from "@/components/ui/Button";
import { queueIsEditable } from "@/queue/defaultQueue";

export interface QueueControlsProps {
  queues: QueueInfo[];
  busy?: boolean;
  onStart: (ids: string[]) => void;
  onStop: (ids: string[]) => void;
  onDelete: (ids: string[]) => void;
  className?: string;
}

function actionableQueues(queues: QueueInfo[]): QueueInfo[] {
  return queues.filter((queue) => queueIsEditable(queue));
}

export function QueueControls({
  queues,
  busy = false,
  onStart,
  onStop,
  onDelete,
  className = "",
}: QueueControlsProps) {
  const { t } = useTranslation();
  const targets = actionableQueues(queues);
  const startIds = targets.filter((queue) => !queue.running).map((queue) => queue.id);
  const stopIds = targets.filter((queue) => queue.running).map((queue) => queue.id);
  const deleteIds = targets.map((queue) => queue.id);

  const anyStartable = startIds.length > 0;
  const anyStoppable = stopIds.length > 0;

  if (targets.length === 0) {
    return null;
  }

  return (
    <div className={`avar-queue-controls ${className}`.trim()}>
      {anyStartable ? (
        <Button
          size="sm"
          variant="secondary"
          loading={busy}
          aria-label={t("queue.start")}
          title={t("queue.start")}
          onClick={() => onStart(startIds)}
        >
          <FontAwesomeIcon icon={faPlay} />
        </Button>
      ) : null}
      {anyStoppable ? (
        <Button
          size="sm"
          variant="secondary"
          loading={busy}
          aria-label={t("queue.stop")}
          title={t("queue.stop")}
          onClick={() => onStop(stopIds)}
        >
          <FontAwesomeIcon icon={faStop} />
        </Button>
      ) : null}
      <Button
        size="sm"
        variant="danger"
        loading={busy}
        aria-label={t("queue.delete")}
        title={t("queue.delete")}
        onClick={() => onDelete(deleteIds)}
      >
        <FontAwesomeIcon icon={faTrash} />
      </Button>
    </div>
  );
}
