import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import type { DownloadInfo } from "@/api/types";
import { ContextMenu, type ContextMenuItem } from "@/components/ui/ContextMenu";
import { useDownloadActions } from "@/hooks/useDownloadActions";
import { canPause, canResume, canStart, canStop } from "@/lib/downloadStatus";
import { openDownloadPopup } from "@/lib/popup";

export interface DownloadContextMenuProps {
  download: DownloadInfo | null;
  position: { x: number; y: number } | null;
  onClose: () => void;
}

export function DownloadContextMenu({ download, position, onClose }: DownloadContextMenuProps) {
  const { t } = useTranslation();
  const actions = useDownloadActions();

  const items = useMemo((): ContextMenuItem[] => {
    if (!download) {
      return [];
    }

    const list: ContextMenuItem[] = [];

    if (canStart(download.status)) {
      list.push({
        id: "start",
        label: t("download.start"),
        onClick: () => void actions.start([download.id]),
      });
    }

    if (canStop(download.status)) {
      list.push({
        id: "stop",
        label: t("download.stop"),
        onClick: () => void actions.stop([download.id]),
      });
    }

    if (canPause(download.status)) {
      list.push({
        id: "pause",
        label: t("download.pause"),
        onClick: () => void actions.pause([download.id]),
      });
    }

    if (canResume(download.status)) {
      list.push({
        id: "resume",
        label: t("download.resume"),
        onClick: () => void actions.resume([download.id]),
      });
    }

    list.push({
      id: "details",
      label: t("download.detailsTitle"),
      onClick: () => void openDownloadPopup(download, t("download.detailsTitle")),
    });

    list.push({
      id: "delete",
      label: t("download.delete"),
      danger: true,
      onClick: () => void actions.removeWithConfirm([download]),
    });

    return list;
  }, [actions, download, t]);

  if (!download || !position || items.length === 0) {
    return null;
  }

  return <ContextMenu x={position.x} y={position.y} items={items} onClose={onClose} />;
}
