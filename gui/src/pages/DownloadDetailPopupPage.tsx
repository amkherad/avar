import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import type { DownloadInfo } from "@/api/types";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";
import { readStashedDownload } from "@/lib/popup";
import { DownloadDetailView } from "@/components/download/DownloadDetailView";
import { Spinner } from "@/components/ui/Spinner";

export interface DownloadDetailPopupPageProps {
  downloadId: string;
}

export function DownloadDetailPopupPage({ downloadId }: DownloadDetailPopupPageProps) {
  const { t } = useTranslation();
  const downloads = useDataStore((s) => s.downloads);
  const client = useConnectionStore((s) => s.client);
  const [download, setDownload] = useState<DownloadInfo | null>(
    () => readStashedDownload(downloadId) ?? downloads.find((d) => d.id === downloadId) ?? null,
  );
  const [loading, setLoading] = useState(!download);

  useEffect(() => {
    document.title = download?.filename
      ? `${download.filename} — ${t("app.title")}`
      : t("download.detailsTitle");
  }, [download, t]);

  useEffect(() => {
    if (download || !client) {
      return;
    }

    let cancelled = false;
    setLoading(true);

    void client.listDownloads().then((items) => {
      if (cancelled) {
        return;
      }
      const found = items.find((d) => d.id === downloadId) ?? null;
      setDownload(found);
      setLoading(false);
    });

    return () => {
      cancelled = true;
    };
  }, [client, download, downloadId]);

  if (loading) {
    return (
      <div className="avar-popup-page">
        <Spinner />
      </div>
    );
  }

  if (!download) {
    return (
      <div className="avar-popup-page">
        <p className="avar-empty">{t("download.notFound")}</p>
      </div>
    );
  }

  return (
    <div className="avar-popup-page">
      <DownloadDetailView download={download} compact />
    </div>
  );
}
