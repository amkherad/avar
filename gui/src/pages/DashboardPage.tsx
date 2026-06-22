import { useCallback, useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faListUl, faPlus } from "@fortawesome/free-solid-svg-icons";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { ShortcutButton } from "@/components/ui/ShortcutButton";
import { ErrorBoundary } from "@/components/ui/ErrorBoundary";
import { Spinner } from "@/components/ui/Spinner";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import { DownloadRow } from "@/components/download/DownloadList";
import { DownloadTable } from "@/components/download/DownloadTable";
import { DownloadToolbar } from "@/components/download/DownloadToolbar";
import { DownloadDetailPanel } from "@/components/download/DownloadDetailPanel";
import { DownloadContextMenu } from "@/components/download/DownloadContextMenu";
import { BatchAddDownloadModal } from "@/components/download/BatchAddDownloadModal";
import { Footer } from "@/components/layout/Footer";
import { ConsolePanel } from "@/components/console/ConsolePanel";
import { useConnectionStore } from "@/stores/connectionStore";
import { useConfigStore } from "@/stores/configStore";
import { useLayoutStore } from "@/stores/layoutStore";
import { createDefaultQueueInfo, withDefaultQueue } from "@/queue/defaultQueue";
import { filterDownloadsBySearch } from "@/lib/downloadSearch";
import {
  collectDownloadStatuses,
  filterDownloadsByStatus,
  sortDownloads,
  type DownloadSort,
  type DownloadStatusFilter,
} from "@/lib/downloadFilterSort";
import { openDownloadPopup } from "@/lib/popup";
import { openAddDownloadDialog } from "@/lib/openAddDownloadDialog";
import { appLogger } from "@/lib/appLogger";
import { useDownloadActions } from "@/hooks/useDownloadActions";
import { useShortcutAction } from "@/shortcuts/useShortcutAction";
import { canPause, canResume } from "@/lib/downloadStatus";
import {
  selectDownloadsForQueue,
  selectEffectiveQueueId,
  selectSelectedDownload,
  selectSelectedDownloads,
  selectSelectedQueue,
  useDataStore,
} from "@/stores/dataStore";
import { restartDataSync } from "@/sync/syncManager";
import type { DownloadInfo } from "@/api/types";

interface DownloadPanelProps {
  staleBanner?: React.ReactNode;
  errorBanner?: React.ReactNode;
}

function DownloadPanel({
  staleBanner,
  errorBanner,
}: DownloadPanelProps) {
  const { t } = useTranslation();
  const queues = useDataStore((s) => s.queues);
  const downloads = useDataStore((s) => s.downloads);
  const status = useDataStore((s) => s.status);
  const queueId = useDataStore(selectEffectiveQueueId);
  const selectedDownloadId = useDataStore((s) => s.selectedDownloadId);
  const selectedDownloadIds = useDataStore((s) => s.selectedDownloadIds);
  const selectDownload = useDataStore((s) => s.selectDownload);
  const setSelectedDownloadId = useDataStore((s) => s.setSelectedDownloadId);
  const setVisibleDownloadOrder = useDataStore((s) => s.setVisibleDownloadOrder);
  const detailPanelMode = useConfigStore((s) => s.config.detailPanelMode);
  const pageSize = useConfigStore((s) => s.config.downloadPageSize);
  const showCheckboxes = useConfigStore((s) => s.config.showDownloadCheckboxes);
  const updateConfig = useConfigStore((s) => s.updateConfig);
  const setSelectedDownloadIds = useDataStore((s) => s.setSelectedDownloadIds);
  const detailPanelOpen = useLayoutStore((s) => s.detailPanelOpen);
  const setDetailPanelOpen = useLayoutStore((s) => s.setDetailPanelOpen);
  const detailPanelWidth = useLayoutStore((s) => s.detailPanelWidth);
  const adjustDetailPanelWidth = useLayoutStore((s) => s.adjustDetailPanelWidth);
  const downloadViewMode = useLayoutStore((s) => s.downloadViewMode);
  const setDownloadViewMode = useLayoutStore((s) => s.setDownloadViewMode);
  const [batchAddOpen, setBatchAddOpen] = useState(false);
  const downloadActions = useDownloadActions();

  const [contextMenu, setContextMenu] = useState<{
    download: DownloadInfo;
    x: number;
    y: number;
  } | null>(null);

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
  const [statusFilter, setStatusFilter] = useState<DownloadStatusFilter>("all");
  const [sort, setSort] = useState<DownloadSort>({ key: null, direction: "asc" });
  const [page, setPage] = useState(1);

  const availableStatuses = useMemo(
    () => collectDownloadStatuses(queueDownloads),
    [queueDownloads],
  );

  const filteredDownloads = useMemo(() => {
    const searched = filterDownloadsBySearch(queueDownloads, searchQuery);
    const statusFiltered = filterDownloadsByStatus(searched, statusFilter);
    return sortDownloads(statusFiltered, sort);
  }, [queueDownloads, searchQuery, statusFilter, sort]);
  const orderedIds = useMemo(
    () => filteredDownloads.map((item) => item.id),
    [filteredDownloads],
  );
  const totalPages = Math.max(1, Math.ceil(filteredDownloads.length / pageSize));

  useEffect(() => {
    setPage(1);
  }, [statusFilter, searchQuery, sort.key, sort.direction]);

  useEffect(() => {
    if (page > totalPages) {
      setPage(totalPages);
    }
  }, [page, totalPages]);

  const pagedDownloads = useMemo(() => {
    const start = (page - 1) * pageSize;
    return filteredDownloads.slice(start, start + pageSize);
  }, [filteredDownloads, page, pageSize]);

  const visibleDownloads = downloadViewMode === "grid" ? filteredDownloads : pagedDownloads;

  useEffect(() => {
    setVisibleDownloadOrder(visibleDownloads.map((item) => item.id));
  }, [visibleDownloads, setVisibleDownloadOrder]);

  useEffect(() => {
    if (queueDownloads.length === 0) {
      if (selectedDownloadId !== null) {
        setSelectedDownloadId(null);
      }
      setDetailPanelOpen(false);
      return;
    }

    const stillValid = queueDownloads.some((d) => d.id === selectedDownloadId);
    if (selectedDownloadId && !stillValid) {
      setSelectedDownloadId(null);
      setDetailPanelOpen(false);
    }
  }, [queueDownloads, selectedDownloadId, setSelectedDownloadId, setDetailPanelOpen]);

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

  function handleToggleSelect(downloadId: string) {
    const next = selectedDownloadIds.includes(downloadId)
      ? selectedDownloadIds.filter((id) => id !== downloadId)
      : [...selectedDownloadIds, downloadId];
    setSelectedDownloadIds(next);
  }

  function handleSelectAll(checked: boolean) {
    if (!checked) {
      setSelectedDownloadIds([]);
      return;
    }
    setSelectedDownloadIds(pagedDownloads.map((item) => item.id));
  }

  function handleSelect(downloadId: string, event?: React.MouseEvent) {
    const additive = event?.ctrlKey || event?.metaKey;
    const range = event?.shiftKey;
    selectDownload(downloadId, { additive, range, orderedIds });

    const { selectedDownloadId, selectedDownloadIds } = useDataStore.getState();
    if (selectedDownloadIds.length > 0 && selectedDownloadId) {
      setDetailPanelOpen(true);
    } else {
      setDetailPanelOpen(false);
    }

    appLogger.gui.debug("Download selected", downloadId);
  }

  function handleOpen(downloadId: string) {
    const download = queueDownloads.find((d) => d.id === downloadId);
    if (download) {
      void openDownloadPopup(download, t("download.detailsTitle"));
    }
  }

  function handleContextMenu(downloadId: string, event: React.MouseEvent) {
    const download = queueDownloads.find((d) => d.id === downloadId);
    if (!download) {
      return;
    }
    handleSelect(downloadId, event);
    setContextMenu({ download, x: event.clientX, y: event.clientY });
  }

  const showPinnedPanel =
    detailPanelMode === "pinned" && detailPanelOpen && selectedDownload !== null;
  const inlinePanel =
    detailPanelMode === "inline" && detailPanelOpen && selectedDownload ? (
      <DownloadDetailPanel download={selectedDownload} pinned={false} />
    ) : null;

  return (
    <div className="avar-dashboard">
      <BatchAddDownloadModal open={batchAddOpen} onClose={() => setBatchAddOpen(false)} />
      <DownloadContextMenu
        download={contextMenu?.download ?? null}
        position={contextMenu ? { x: contextMenu.x, y: contextMenu.y } : null}
        onClose={() => setContextMenu(null)}
      />

      <div className="avar-dashboard__workspace">
        <div className="avar-dashboard__main">
          <Card
            title={
              selectedQueue
                ? `${t("download.title")} — ${selectedQueue.name}`
                : t("download.title")
            }
            actions={
              <>
                <Button size="sm" variant="secondary" onClick={() => setBatchAddOpen(true)}>
                  <FontAwesomeIcon icon={faListUl} />
                  {t("download.batchAdd.button")}
                </Button>
                <ShortcutButton
                  size="sm"
                  variant="primary"
                  shortcut="download.add"
                  onClick={() => openAddDownloadDialog(t("download.add"), queueId)}
                >
                  <FontAwesomeIcon icon={faPlus} />
                  {t("download.add")}
                </ShortcutButton>
              </>
            }
          >
            {staleBanner}
            {errorBanner}

            <DownloadToolbar
              searchQuery={searchQuery}
              onSearchChange={setSearchQuery}
              statusFilter={statusFilter}
              onStatusFilterChange={setStatusFilter}
              availableStatuses={availableStatuses}
              showStatusFilter={downloadViewMode === "grid"}
              viewMode={downloadViewMode}
              onViewModeChange={setDownloadViewMode}
              selectedDownloads={selectedDownloads}
            />

            <div
              className={`avar-download-list${downloadViewMode === "grid" ? " avar-download-list--cards" : ""}`}
            >
              {status === "loading" && queueDownloads.length === 0 ? <Spinner /> : null}

              {downloadViewMode === "grid" ? (
                status !== "loading" && filteredDownloads.length === 0 ? (
                  <p className="avar-empty">
                    {searchQuery ? t("download.searchEmpty") : t("download.empty")}
                  </p>
                ) : (
                  filteredDownloads.map((item) => (
                    <DownloadRow
                      key={item.id || item.filename}
                      download={item}
                      selected={selectedDownloadIds.includes(item.id)}
                      onSelect={(event) => handleSelect(item.id, event)}
                      onOpen={() => handleOpen(item.id)}
                      onContextMenu={(event) => handleContextMenu(item.id, event)}
                    />
                  ))
                )
              ) : (
                <DownloadTable
                  downloads={pagedDownloads}
                  selectedIds={selectedDownloadIds}
                  loading={status === "loading" && queueDownloads.length === 0}
                  emptyMessage={
                    searchQuery || statusFilter !== "all"
                      ? t("download.searchEmpty")
                      : t("download.empty")
                  }
                  showCheckboxes={showCheckboxes}
                  page={page}
                  pageSize={pageSize}
                  totalItems={filteredDownloads.length}
                  statusFilter={statusFilter}
                  onStatusFilterChange={setStatusFilter}
                  availableStatuses={availableStatuses}
                  sort={sort}
                  onSortChange={setSort}
                  onPageChange={setPage}
                  onPageSizeChange={(size) => {
                    updateConfig({ downloadPageSize: size });
                    setPage(1);
                  }}
                  onSelect={(id, event) => handleSelect(id, event)}
                  onToggleSelect={handleToggleSelect}
                  onSelectAll={handleSelectAll}
                  onOpen={(id) => handleOpen(id)}
                  onContextMenu={handleContextMenu}
                  viewMode={downloadViewMode}
                  onViewModeChange={setDownloadViewMode}
                  onToggleCheckboxes={() =>
                    updateConfig({ showDownloadCheckboxes: !showCheckboxes })
                  }
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

  const staleBanner =
    connection !== "connected" && hasData ? (
      <div className="avar-stale-banner avar-dashboard__banner" role="status">
        {t("common.staleData")}
      </div>
    ) : null;

  const errorBanner = error ? (
    <div className="avar-error-banner avar-dashboard__banner">
      <span>{error}</span>
      <Button size="sm" variant="secondary" onClick={() => restartDataSync()}>
        {t("common.retry")}
      </Button>
    </div>
  ) : null;

  return (
    <ErrorBoundary name={t("download.title")} resetLabel={t("common.tryAgain")}>
      <DownloadPanel
        staleBanner={staleBanner}
        errorBanner={errorBanner}
      />
    </ErrorBoundary>
  );
}
