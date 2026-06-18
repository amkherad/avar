import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import {
  faCircleInfo,
  faPause,
  faPlay,
  faStop,
  faTrash,
} from "@fortawesome/free-solid-svg-icons";
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

    return [
      {
        id: "start",
        label: t("download.start"),
        icon: faPlay,
        disabled: !canStart(download.status) || actions.busy,
        onClick: () => void actions.start([download.id]),
      },
      {
        id: "stop",
        label: t("download.stop"),
        icon: faStop,
        disabled: !canStop(download.status) || actions.busy,
        onClick: () => void actions.stop([download.id]),
      },
      {
        id: "pause",
        label: t("download.pause"),
        icon: faPause,
        disabled: !canPause(download.status) || actions.busy,
        onClick: () => void actions.pause([download.id]),
      },
      {
        id: "resume",
        label: t("download.resume"),
        icon: faPlay,
        disabled: !canResume(download.status) || actions.busy,
        onClick: () => void actions.resume([download.id]),
      },
      {
        id: "details",
        label: t("download.detailsTitle"),
        icon: faCircleInfo,
        onClick: () => void openDownloadPopup(download, t("download.detailsTitle")),
      },
      {
        id: "delete",
        label: t("download.delete"),
        icon: faTrash,
        disabled: actions.busy,
        danger: true,
        onClick: () => void actions.removeWithConfirm([download]),
      },
    ];
  }, [actions, download, t]);

  if (!download || !position) {
    return null;
  }

  return <ContextMenu x={position.x} y={position.y} items={items} onClose={onClose} />;
}
