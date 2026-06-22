import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import {
  faCircleInfo,
  faDownload,
  faPause,
  faPlay,
  faRotateRight,
  faStop,
  faTrash,
} from "@fortawesome/free-solid-svg-icons";
import type { DownloadInfo } from "@/api/types";
import { ContextMenu, type ContextMenuItem } from "@/components/ui/ContextMenu";
import { useDownloadActions } from "@/hooks/useDownloadActions";
import {
  canPause,
  canResume,
  canStart,
  canStop,
  canRedownload,
  isCompleted,
} from "@/lib/downloadStatus";
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

    const menuItems: ContextMenuItem[] = [];

    if (canStart(download.status)) {
      menuItems.push({
        id: "start",
        label: t("download.start"),
        icon: faPlay,
        disabled: actions.busy,
        onClick: () => void actions.start([download.id]),
      });
    }

    if (canStop(download.status)) {
      menuItems.push({
        id: "stop",
        label: t("download.stop"),
        icon: faStop,
        disabled: actions.busy,
        onClick: () => void actions.stop([download.id]),
      });
    }

    if (canPause(download.status)) {
      menuItems.push({
        id: "pause",
        label: t("download.pause"),
        icon: faPause,
        disabled: actions.busy,
        onClick: () => void actions.pause([download.id]),
      });
    }

    if (canResume(download.status)) {
      menuItems.push({
        id: "resume",
        label: t("download.resume"),
        icon: faPlay,
        disabled: actions.busy,
        onClick: () => void actions.resume([download.id]),
      });
    }

    if (canRedownload(download.status) && download.url) {
      menuItems.push({
        id: "redownload",
        label: t("download.redownload"),
        icon: faRotateRight,
        disabled: actions.busy,
        onClick: () => void actions.redownload([download]),
      });
    }

    if (actions.copyToLocalVisible && isCompleted(download.status)) {
      menuItems.push({
        id: "copyToLocal",
        label: t("download.copyToLocal"),
        icon: faDownload,
        disabled: !actions.copyToLocalAvailable || actions.busy,
        onClick: () => void actions.copyToLocal([download]),
      });
    }

    menuItems.push(
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
    );

    return menuItems;
  }, [actions, download, t]);

  if (!download || !position) {
    return null;
  }

  return <ContextMenu x={position.x} y={position.y} items={items} onClose={onClose} />;
}
