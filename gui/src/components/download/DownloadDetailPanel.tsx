import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faThumbtack } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import type { DownloadInfo } from "@/api/types";
import { useConfigStore } from "@/stores/configStore";
import { DownloadDetailView } from "./DownloadDetailView";
import { DownloadControls } from "./DownloadControls";
import { openDownloadPopup } from "@/lib/popup";
import { appLogger } from "@/lib/appLogger";

export interface DownloadDetailPanelProps {
  download: DownloadInfo | null;
  pinned?: boolean;
}

export function DownloadDetailPanel({ download, pinned = true }: DownloadDetailPanelProps) {
  const { t } = useTranslation();
  const detailPanelMode = useConfigStore((s) => s.config.detailPanelMode);
  const updateConfig = useConfigStore((s) => s.updateConfig);

  if (!download) {
    return (
      <aside className={`avar-download-panel ${pinned ? "avar-download-panel--pinned" : ""}`}>
        <p className="avar-empty">{t("download.selectHint")}</p>
      </aside>
    );
  }

  function handleOpenPopup() {
    appLogger.gui.debug("Open download popup from detail panel", download!.id);
    void openDownloadPopup(download!, t("download.detailsTitle"));
  }

  function toggleMode() {
    appLogger.gui.debug("Toggle detail panel mode");
    updateConfig({
      detailPanelMode: detailPanelMode === "pinned" ? "inline" : "pinned",
    });
  }

  return (
    <aside className={`avar-download-panel ${pinned ? "avar-download-panel--pinned" : ""}`}>
      <header className="avar-download-panel__header">
        <h2 className="avar-download-panel__title">{t("download.detailsTitle")}</h2>
        <div className="avar-download-panel__header-actions">
          <DownloadControls downloads={[download]} />
          <Button
            size="sm"
            variant="ghost"
            aria-label={t("download.togglePanelMode")}
            title={t("download.togglePanelMode")}
            onClick={toggleMode}
          >
            <FontAwesomeIcon icon={faThumbtack} />
          </Button>
        </div>
      </header>
      <DownloadDetailView download={download} onOpenPopup={handleOpenPopup} />
    </aside>
  );
}
