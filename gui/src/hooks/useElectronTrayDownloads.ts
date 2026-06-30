import { useEffect, useMemo, useRef } from "react";
import { useTranslation } from "react-i18next";
import { useDataStore } from "@/stores/dataStore";
import { progressPercent } from "@/components/download/format";

const MAX_TRAY_DOWNLOADS = 3;

export function useElectronTrayDownloads(): void {
  const { t } = useTranslation();
  const downloads = useDataStore((s) => s.downloads);
  const lastPayloadRef = useRef<string | null>(null);

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

  const sectionLabel = t("tray.activeDownloads");

  useEffect(() => {
    if (!window.avar?.isElectron) {
      return;
    }

    const payloadKey = JSON.stringify({ sectionLabel, items: activeItems });
    if (payloadKey === lastPayloadRef.current) {
      return;
    }

    lastPayloadRef.current = payloadKey;
    void window.avar.setTrayActiveDownloads({
      sectionLabel,
      items: activeItems,
    });
  }, [activeItems, sectionLabel]);
}
