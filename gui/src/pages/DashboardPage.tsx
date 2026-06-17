import { useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faArrowDown } from "@fortawesome/free-solid-svg-icons";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Spinner } from "@/components/ui/Spinner";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import { DownloadRow } from "@/components/download/DownloadList";
import { DownloadTable } from "@/components/download/DownloadTable";
import { DownloadToolbar } from "@/components/download/DownloadToolbar";
import { DownloadDetailPanel } from "@/components/download/DownloadDetailPanel";
import { Footer } from "@/components/layout/Footer";
import { ConsolePanel } from "@/components/console/ConsolePanel";
import { useConnectionStore } from "@/stores/connectionStore";
import { useConfigStore } from "@/stores/configStore";
import { useLayoutStore } from "@/stores/layoutStore";
import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";
import { filterDownloadsBySearch } from "@/lib/downloadSearch";
import {
  selectDownloadsForQueue,
  selectEffectiveQueueId,
  selectSelectedDownload,
  selectSelectedQueue,
  useDataStore,
} from "@/stores/dataStore";
import { restartDataSync } from "@/sync/syncManager";
import { openDownloadPopup } from "@/lib/popup";
import { appLogger } from "@/lib/appLogger";

function DownloadPanel() {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const queues = useDataStore((s) => s.queues);
  const downloads = useDataStore((s) => s.downloads);
  const status = useDataStore((s) => s.status);
  const queueId = useDataStore(selectEffectiveQueueId);
  const selectedDownloadId = useDataStore((s) => s.selectedDownloadId);
  const setSelectedDownloadId = useDataStore((s) => s.setSelectedDownloadId);
  const detailPanelMode = useConfigStore((s) => s.config.detailPanelMode);
  const detailPanelOpen = useLayoutStore((s) => s.detailPanelOpen);
  const detailPanelWidth = useLayoutStore((s) => s.detailPanelWidth);
  const adjustDetailPanelWidth = useLayoutStore((s) => s.adjustDetailPanelWidth);
  const downloadViewMode = useLayoutStore((s) => s.downloadViewMode);
  const setDownloadViewMode = useLayoutStore((s) => s.setDownloadViewMode);

  const displayQueues = withDefaultQueue(
    queues,
    createDefaultQueueInfo(t("queue.defaultName"), t("queue.defaultDescription")),
  );
  const selectedQueue = useMemo(
    () => selectSelectedQueue(displayQueues, queueId),
    [displayQueues, queueId],
  );
  const queueDownloads = useMemo(
    () => selectDownloadsForQueue(downloads, queueId),
    [downloads, queueId],
  );
  const selectedDownload = useMemo(
    () => selectSelectedDownload(queueDownloads, selectedDownloadId),
    [queueDownloads, selectedDownloadId],
  );

  const [addUrl, setAddUrl] = useState("");
  const [adding, setAdding] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");

  const filteredDownloads = useMemo(
    () => filterDownloadsBySearch(queueDownloads, searchQuery),
    [queueDownloads, searchQuery],
  );

  useEffect(() => {
    if (queueDownloads.length === 0) {
      if (selectedDownloadId !== null) {
        setSelectedDownloadId(null);
      }
      return;
    }

    const stillValid = queueDownloads.some((d) => d.id === selectedDownloadId);
    if (!selectedDownloadId || !stillValid) {
      setSelectedDownloadId(queueDownloads[0].id);
    }
  }, [queueDownloads, selectedDownloadId, setSelectedDownloadId]);

  async function handleAddDownload() {
    if (!client || !addUrl.trim()) {
      return;
    }
    setAdding(true);
    try {
      await client.addDownload(
        addUrl.trim(),
        isDefaultQueue(queueId) ? undefined : selectedQueue?.name,
      );
      setAddUrl("");
      appLogger.gui.info(t("download.added", { url: addUrl.trim() }));
      await useDataStore.getState().refresh();
    } catch (err) {
      appLogger.gui.error(
        t("download.addFailed"),
        err instanceof Error ? err.message : undefined,
      );
    } finally {
      setAdding(false);
    }
  }

  function handleDownloadClick(downloadId: string) {
    appLogger.gui.debug("Download row opened", downloadId);
    setSelectedDownloadId(downloadId);
    const download = queueDownloads.find((d) => d.id === downloadId);
    if (download) {
      void openDownloadPopup(download, t("download.detailsTitle"));
    }
  }

  function handleSelect(downloadId: string) {
    appLogger.gui.debug("Download selected", downloadId);
    setSelectedDownloadId(downloadId);
  }

  const showPinnedPanel = detailPanelMode === "pinned" && detailPanelOpen;
  const inlinePanel =
    detailPanelMode === "inline" && detailPanelOpen ? (
      <DownloadDetailPanel download={selectedDownload} pinned={false} />
    ) : null;

  return (
    <div className="avar-dashboard">
      <div className="avar-dashboard__workspace">
        <div className="avar-dashboard__main">
          <Card
            title={
              selectedQueue
                ? `${t("download.title")} — ${selectedQueue.name}`
                : t("download.title")
            }
          >
            <div className="avar-download-add">
              <Input
                label={t("download.url")}
                value={addUrl}
                onChange={(e) => setAddUrl(e.target.value)}
                placeholder="https://"
              />
              <div className="avar-download-add__action">
                <Button loading={adding} onClick={() => void handleAddDownload()}>
                  <FontAwesomeIcon icon={faArrowDown} />
                  {t("download.submit")}
                </Button>
              </div>
            </div>

            <DownloadToolbar
              searchQuery={searchQuery}
              onSearchChange={setSearchQuery}
              viewMode={downloadViewMode}
              onViewModeChange={setDownloadViewMode}
              selectedDownload={selectedDownload}
            />

            <div className="avar-download-list">
              {status === "loading" && queueDownloads.length === 0 ? <Spinner /> : null}
              {status !== "loading" && filteredDownloads.length === 0 ? (
                <p className="avar-empty">
                  {searchQuery ? t("download.searchEmpty") : t("download.empty")}
                </p>
              ) : null}

              {downloadViewMode === "grid" ? (
                filteredDownloads.map((item) => (
                  <DownloadRow
                    key={item.id || item.filename}
                    download={item}
                    selected={item.id === selectedDownloadId}
                    onSelect={() => handleSelect(item.id)}
                    onOpen={() => handleDownloadClick(item.id)}
                  />
                ))
              ) : (
                <DownloadTable
                  downloads={filteredDownloads}
                  selectedId={selectedDownloadId}
                  onSelect={handleSelect}
                  onOpen={handleDownloadClick}
                />
              )}
            </div>

            {inlinePanel}
          </Card>
        </div>

        {showPinnedPanel ? (
          <>
            <ResizeHandle
              axis="horizontal"
              min={220}
              max={560}
              invert
              label={t("layout.resizeDetailPanel")}
              onResize={adjustDetailPanelWidth}
            />
            <div
              className="avar-download-panel-wrap"
              style={{ width: detailPanelWidth }}
            >
              <DownloadDetailPanel download={selectedDownload} pinned />
            </div>
          </>
        ) : null}
      </div>

      <Footer />
      <ConsolePanel />
    </div>
  );
}

export function DashboardPage() {
  const { t } = useTranslation();
  const connection = useConnectionStore((s) => s.connection);
  const error = useDataStore((s) => s.error);
  const hasData = useDataStore(
    (s) => s.queues.length > 0 || s.downloads.length > 0 || s.health !== null,
  );

  return (
    <>
      {connection !== "connected" && hasData ? (
        <div className="avar-stale-banner" role="status">
          {t("common.staleData")}
        </div>
      ) : null}

      {error ? (
        <div className="avar-error-banner">
          <span>{error}</span>
          <Button size="sm" variant="secondary" onClick={() => restartDataSync()}>
            {t("common.retry")}
          </Button>
        </div>
      ) : null}

      <DownloadPanel />
    </>
  );
}
