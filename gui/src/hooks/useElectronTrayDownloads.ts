import { useEffect, useMemo } from "react";
import { useTranslation } from "react-i18next";
import { useDataStore } from "@/stores/dataStore";
import { progressPercent } from "@/components/download/format";

const MAX_TRAY_DOWNLOADS = 3;

export function useElectronTrayDownloads(): void {
  const { t } = useTranslation();
  const downloads = useDataStore((s) => s.downloads);

  const activeItems = useMemo(() => {
    return downloads
      .filter((item) => item.status === "downloading")
      .sort((a, b) => a.id.localeCompare(b.id))
      .slice(0, MAX_TRAY_DOWNLOADS)
      .map((item) => ({
        id: item.id,
        filename: item.filename,
        percent: progressPercent(item.bytesDownloaded, item.totalBytes),
      }));
  }, [downloads]);

  useEffect(() => {
    if (!window.avar?.isElectron) {
      return;
    }

    void window.avar.setTrayActiveDownloads({
      sectionLabel: t("tray.activeDownloads"),
      items: activeItems,
    });
  }, [activeItems, t]);
}
