import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import {
  faCircleInfo,
  faDownload,
  faFolder,
  faFolderOpen,
  faLink,
  faPause,
  faPlay,
  faRightLeft,
  faRotateRight,
  faStop,
  faTrash,
} from "@fortawesome/free-solid-svg-icons";
import type { DownloadInfo, QueueInfo } from "@/api/types";
import { ContextMenu, type ContextMenuItem } from "@/components/ui/ContextMenu";
import { useDownloadActions } from "@/hooks/useDownloadActions";
import {
  canPause,
  canResume,
  canStart,
  canStop,
  canRedownload,
  canRefreshLink,
  isCompleted,
} from "@/lib/downloadStatus";
import { openDownloadPopup } from "@/lib/popup";

export interface DownloadContextMenuProps {
  downloads: DownloadInfo[];
  targetQueues: QueueInfo[];
  position: { x: number; y: number } | null;
  onClose: () => void;
  onRefreshLink?: (download: DownloadInfo) => void;
}

export function DownloadContextMenu({
  downloads,
  targetQueues,
  position,
  onClose,
  onRefreshLink,
}: DownloadContextMenuProps) {
  const { t } = useTranslation();
  const actions = useDownloadActions();
  const batchMode = downloads.length > 1;
  const ids = downloads.map((item) => item.id);

  const items = useMemo((): ContextMenuItem[] => {
    if (downloads.length === 0) {
      return [];
    }

    if (batchMode) {
      const menuItems: ContextMenuItem[] = [];

      const anyStartable = downloads.some(
        (item) => canStart(item.status) && !canResume(item.status),
      );
      const anyStoppable = downloads.some((item) => canStop(item.status));
      const anyPausable = downloads.some((item) => canPause(item.status));
      const anyResumable = downloads.some((item) => canResume(item.status));
      const anyRedownloadable = downloads.some((item) => canRedownload(item.status));
      const anyCopyToLocal =
        actions.copyToLocalVisible && downloads.some((item) => isCompleted(item.status));

      if (anyStartable) {
        menuItems.push({
          id: "start",
          label: t("download.start"),
          icon: faPlay,
          disabled: actions.busy,
          onClick: () => void actions.start(ids),
        });
      }

      if (anyStoppable) {
        menuItems.push({
          id: "stop",
          label: t("download.stop"),
          icon: faStop,
          disabled: actions.busy,
          onClick: () => void actions.stop(ids),
        });
      }

      if (anyPausable) {
        menuItems.push({
          id: "pause",
          label: t("download.pause"),
          icon: faPause,
          disabled: actions.busy,
          onClick: () => void actions.pause(ids),
        });
      }

      if (anyResumable) {
        menuItems.push({
          id: "resume",
          label: t("download.resume"),
          icon: faPlay,
          disabled: actions.busy,
          onClick: () => void actions.resume(ids),
        });
      }

      if (anyRedownloadable) {
        menuItems.push({
          id: "redownload",
          label: t("download.redownload"),
          icon: faRotateRight,
          disabled: actions.busy,
          onClick: () => void actions.redownload(downloads),
        });
      }

      if (anyCopyToLocal) {
        menuItems.push({
          id: "copyToLocal",
          label: t("download.copyToLocal"),
          icon: faDownload,
          disabled: !actions.copyToLocalAvailable || actions.busy,
          onClick: () =>
            void actions.copyToLocal(downloads.filter((item) => isCompleted(item.status))),
        });
      }

      if (targetQueues.length > 0) {
        menuItems.push({
          id: "moveToQueue",
          label: t("download.moveToQueue"),
          icon: faRightLeft,
          disabled: actions.busy,
          children: targetQueues.map((queue, index) => ({
            id: `moveToQueue-${queue.id}`,
            label: queue.name,
            checked: index === 0,
            disabled: actions.busy,
            onClick: () => void actions.moveToQueue(ids, queue),
          })),
        });
      }

      menuItems.push({
        id: "delete",
        label: t("download.delete"),
        icon: faTrash,
        disabled: actions.busy,
        danger: true,
        onClick: () => void actions.removeWithConfirm(downloads),
      });

      return menuItems;
    }

    const download = downloads[0];
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

    if (canRefreshLink(download.status) && onRefreshLink) {
      menuItems.push({
        id: "refreshLink",
        label: t("download.refreshLink"),
        icon: faLink,
        disabled: actions.busy,
        onClick: () => onRefreshLink(download),
      });
    }

    if (canRedownload(download.status)) {
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

    if (actions.openFileVisible && isCompleted(download.status)) {
      menuItems.push({
        id: "openFile",
        label: t("download.openFile"),
        icon: faFolderOpen,
        disabled: actions.busy,
        onClick: () => void actions.openFile([download]),
      });
      menuItems.push({
        id: "openContainingFolder",
        label: t("download.openContainingFolder"),
        icon: faFolder,
        disabled: actions.busy,
        onClick: () => void actions.openContainingFolder([download]),
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
  }, [
    actions,
    batchMode,
    downloads,
    ids,
    onRefreshLink,
    t,
    targetQueues,
  ]);

  if (downloads.length === 0 || !position) {
    return null;
  }

  return <ContextMenu x={position.x} y={position.y} items={items} onClose={onClose} />;
}
