import { useCallback, useEffect, useMemo, useState } from "react";

import { useTranslation } from "react-i18next";

import { Card } from "@/components/ui/Card";

import { Button } from "@/components/ui/Button";

import { Spinner } from "@/components/ui/Spinner";

import { ResizeHandle } from "@/components/ui/ResizeHandle";

import { DownloadRow } from "@/components/download/DownloadList";

import { DownloadTable } from "@/components/download/DownloadTable";

import { DownloadToolbar } from "@/components/download/DownloadToolbar";

import { DownloadDetailPanel } from "@/components/download/DownloadDetailPanel";

import { AddDownloadModal, useAddDownloadModal } from "@/components/download/AddDownloadModal";

import { Footer } from "@/components/layout/Footer";

import { ConsolePanel } from "@/components/console/ConsolePanel";

import { useConnectionStore } from "@/stores/connectionStore";

import { useConfigStore } from "@/stores/configStore";

import { useLayoutStore } from "@/stores/layoutStore";

import { createDefaultQueueInfo, withDefaultQueue } from "@/queue/defaultQueue";

import { filterDownloadsBySearch } from "@/lib/downloadSearch";

import { openDownloadPopup } from "@/lib/popup";

import { appLogger } from "@/lib/appLogger";

import { useDownloadActions } from "@/hooks/useDownloadActions";

import { useShortcutAction } from "@/shortcuts/useShortcutAction";

import {

  canPause,

  canResume,

  canStart,

  canStop,

} from "@/lib/downloadStatus";

import {

  selectDownloadsForQueue,

  selectEffectiveQueueId,

  selectSelectedDownload,

  selectSelectedDownloads,

  selectSelectedQueue,

  useDataStore,

} from "@/stores/dataStore";

import { restartDataSync } from "@/sync/syncManager";



function DownloadPanel() {

  const { t } = useTranslation();

  const queues = useDataStore((s) => s.queues);

  const downloads = useDataStore((s) => s.downloads);

  const status = useDataStore((s) => s.status);

  const queueId = useDataStore(selectEffectiveQueueId);

  const selectedDownloadId = useDataStore((s) => s.selectedDownloadId);

  const selectedDownloadIds = useDataStore((s) => s.selectedDownloadIds);

  const selectDownload = useDataStore((s) => s.selectDownload);

  const setSelectedDownloadId = useDataStore((s) => s.setSelectedDownloadId);

  const detailPanelMode = useConfigStore((s) => s.config.detailPanelMode);

  const detailPanelOpen = useLayoutStore((s) => s.detailPanelOpen);

  const detailPanelWidth = useLayoutStore((s) => s.detailPanelWidth);

  const adjustDetailPanelWidth = useLayoutStore((s) => s.adjustDetailPanelWidth);

  const downloadViewMode = useLayoutStore((s) => s.downloadViewMode);

  const setDownloadViewMode = useLayoutStore((s) => s.setDownloadViewMode);

  const { open, openModal, closeModal } = useAddDownloadModal();

  const downloadActions = useDownloadActions();



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

  const selectedDownloads = useMemo(

    () => selectSelectedDownloads(queueDownloads, selectedDownloadIds),

    [queueDownloads, selectedDownloadIds],

  );



  const [searchQuery, setSearchQuery] = useState("");



  const filteredDownloads = useMemo(

    () => filterDownloadsBySearch(queueDownloads, searchQuery),

    [queueDownloads, searchQuery],

  );

  const orderedIds = useMemo(

    () => filteredDownloads.map((item) => item.id),

    [filteredDownloads],

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



  const runOnSelection = useCallback(

    (handler: (items: typeof selectedDownloads) => void) => {

      if (selectedDownloads.length === 0) {

        return;

      }

      handler(selectedDownloads);

    },

    [selectedDownloads],

  );



  useShortcutAction("download.pause", () => {

    runOnSelection((items) => {

      const ids = items.map((item) => item.id);

      if (items.some((item) => canResume(item.status))) {

        void downloadActions.resume(ids);

      } else if (items.some((item) => canPause(item.status))) {

        void downloadActions.pause(ids);

      }

    });

  });



  useShortcutAction("download.start", () => {

    runOnSelection((items) => {

      void downloadActions.start(items.map((item) => item.id));

    });

  });



  useShortcutAction("download.stop", () => {

    runOnSelection((items) => {

      void downloadActions.stop(items.map((item) => item.id));

    });

  });



  useShortcutAction("download.delete", () => {

    runOnSelection((items) => {

      void downloadActions.removeWithConfirm(items);

    });

  });



  function handleSelect(downloadId: string, event?: React.MouseEvent) {

    const additive = event?.ctrlKey || event?.metaKey;

    const range = event?.shiftKey;

    selectDownload(downloadId, { additive, range, orderedIds });

    appLogger.gui.debug("Download selected", downloadId);

  }



  function handleOpen(downloadId: string) {

    const download = queueDownloads.find((d) => d.id === downloadId);

    if (download) {

      void openDownloadPopup(download, t("download.detailsTitle"));

    }

  }



  const showPinnedPanel = detailPanelMode === "pinned" && detailPanelOpen;

  const inlinePanel =

    detailPanelMode === "inline" && detailPanelOpen ? (

      <DownloadDetailPanel download={selectedDownload} pinned={false} />

    ) : null;



  return (

    <div className="avar-dashboard">

      <AddDownloadModal open={open} onClose={closeModal} />



      <div className="avar-dashboard__workspace">

        <div className="avar-dashboard__main">

          <Card

            title={

              selectedQueue

                ? `${t("download.title")} — ${selectedQueue.name}`

                : t("download.title")

            }

          >

            <DownloadToolbar

              searchQuery={searchQuery}

              onSearchChange={setSearchQuery}

              viewMode={downloadViewMode}

              onViewModeChange={setDownloadViewMode}

              selectedDownloads={selectedDownloads}

              onAddDownload={openModal}

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

                    selected={selectedDownloadIds.includes(item.id)}

                    onSelect={(event) => handleSelect(item.id, event)}

                    onOpen={() => handleOpen(item.id)}

                  />

                ))

              ) : (

                <DownloadTable

                  downloads={filteredDownloads}

                  selectedIds={selectedDownloadIds}

                  onSelect={(id, event) => handleSelect(id, event)}

                  onOpen={(id) => handleOpen(id)}

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


