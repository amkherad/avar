import { useMemo, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import {
  faEllipsisVertical,
  faFileArrowDown,
  faFileArrowUp,
} from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { ContextMenu, type ContextMenuItem } from "@/components/ui/ContextMenu";
import type { DownloadInfo } from "@/api/types";
import {
  collectDownloadUrls,
  exportDownloadUrlsToTextFile,
} from "@/lib/downloadListTextIo";
import { appLogger } from "@/lib/appLogger";
import { showNotification } from "@/lib/notificationService";

export interface DownloadListActionsMenuProps {
  downloads: DownloadInfo[];
  selectedDownloads: DownloadInfo[];
  onImportFromText: () => void;
}

export function DownloadListActionsMenu({
  downloads,
  selectedDownloads,
  onImportFromText,
}: DownloadListActionsMenuProps) {
  const { t } = useTranslation();
  const triggerRef = useRef<HTMLDivElement>(null);
  const [menuPosition, setMenuPosition] = useState<{ x: number; y: number } | null>(null);

  const exportSource = selectedDownloads.length > 0 ? selectedDownloads : downloads;
  const exportUrls = useMemo(() => collectDownloadUrls(exportSource), [exportSource]);

  const items = useMemo((): ContextMenuItem[] => {
    return [
      {
        id: "export-text",
        label: t("download.listActions.exportToText"),
        icon: faFileArrowDown,
        disabled: exportUrls.length === 0,
        onClick: () => {
          exportDownloadUrlsToTextFile(exportUrls);
          appLogger.gui.debug("Download list exported to text", exportUrls.length);
          void showNotification({
            title: t("download.listActions.exported"),
            body: t("download.listActions.exportedCount", { count: exportUrls.length }),
            category: "general",
          });
        },
      },
      {
        id: "import-text",
        label: t("download.listActions.importFromText"),
        icon: faFileArrowUp,
        onClick: () => {
          appLogger.gui.debug("Download list import from text opened");
          onImportFromText();
        },
      },
    ];
  }, [exportUrls, onImportFromText, t]);

  function openMenu() {
    const rect = triggerRef.current?.getBoundingClientRect();
    if (!rect) {
      return;
    }
    setMenuPosition({ x: rect.left, y: rect.bottom + 4 });
  }

  function closeMenu() {
    setMenuPosition(null);
  }

  return (
    <>
      <div ref={triggerRef} className="avar-download-list-actions">
        <Button
          size="sm"
          variant="secondary"
          aria-label={t("download.listActions.menu")}
          title={t("download.listActions.menu")}
          aria-haspopup="menu"
          aria-expanded={menuPosition !== null}
          onClick={() => {
            if (menuPosition) {
              closeMenu();
              return;
            }
            openMenu();
          }}
        >
          <FontAwesomeIcon icon={faEllipsisVertical} />
        </Button>
      </div>

      {menuPosition ? (
        <ContextMenu x={menuPosition.x} y={menuPosition.y} items={items} onClose={closeMenu} />
      ) : null}
    </>
  );
}
