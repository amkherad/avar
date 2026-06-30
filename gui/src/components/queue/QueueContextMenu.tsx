import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import { faPenToSquare, faPlay, faStop, faTrash } from "@fortawesome/free-solid-svg-icons";
import type { QueueInfo } from "@/api/types";
import { ContextMenu, type ContextMenuItem } from "@/components/ui/ContextMenu";
import {
  isDefaultQueue,
  queueHasLifecycleActions,
  queueIsEditable,
} from "@/queue/defaultQueue";

export interface QueueContextMenuProps {
  queue: QueueInfo | null;
  position: { x: number; y: number } | null;
  showDelete?: boolean;
  showModify?: boolean;
  busy?: boolean;
  onClose: () => void;
  onStart: (id: string) => void;
  onStop: (id: string) => void;
  onDelete: (id: string) => void;
  onModify?: (id: string) => void;
}

export function QueueContextMenu({
  queue,
  position,
  showDelete = false,
  showModify = false,
  busy = false,
  onClose,
  onStart,
  onStop,
  onDelete,
  onModify,
}: QueueContextMenuProps) {
  const { t } = useTranslation();

  const items = useMemo((): ContextMenuItem[] => {
    if (!queue || !queueHasLifecycleActions(queue)) {
      return [];
    }

    const isDefault = isDefaultQueue(queue.id);
    const list: ContextMenuItem[] = [
      {
        id: "start",
        label: t("queue.start"),
        icon: faPlay,
        disabled: busy || queue.running,
        onClick: () => onStart(queue.id),
      },
      {
        id: "stop",
        label: t("queue.stop"),
        icon: faStop,
        disabled: busy || !queue.running,
        onClick: () => onStop(queue.id),
      },
    ];

    if (!isDefault && showModify && onModify) {
      list.push({
        id: "modify",
        label: t("queue.modify"),
        icon: faPenToSquare,
        disabled: busy,
        onClick: () => onModify(queue.id),
      });
    }

    if (!isDefault && showDelete) {
      list.push({
        id: "delete",
        label: t("queue.delete"),
        icon: faTrash,
        disabled: busy,
        danger: true,
        onClick: () => onDelete(queue.id),
      });
    }

    return list;
  }, [busy, onDelete, onModify, onStart, onStop, queue, showDelete, showModify, t]);

  if (!queue || !position || items.length === 0) {
    return null;
  }

  return <ContextMenu x={position.x} y={position.y} items={items} onClose={onClose} />;
}
