import { useEffect, useMemo, useRef, useState, type MouseEvent } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faListCheck, faPlay } from "@fortawesome/free-solid-svg-icons";
import type { BatchAddDownloadItem } from "@/lib/batchAddDownloads";
import {
  formatBatchFileType,
  loadBatchAddPayload,
  type BatchAddDownloadsPayload,
} from "@/lib/batchAddDownloads";
import { useUnitFormat } from "@/hooks/useUnitFormat";
import { appLogger } from "@/lib/appLogger";
import { applyTableSelection } from "@/lib/tableSelection";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore, selectEffectiveQueueId, selectSelectedQueue } from "@/stores/dataStore";
import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";
import { Button } from "@/components/ui/Button";
import { DataTable, type DataTableColumn } from "@/components/ui/DataTable";
import { Input } from "@/components/ui/Input";
import { Select } from "@/components/ui/Select";
import { Spinner } from "@/components/ui/Spinner";

export interface BatchAddDownloadsPopupPageProps {
  batchId: string;
}

function truncateUrl(url: string, max = 72): string {
  if (url.length <= max) {
    return url;
  }
  const head = Math.max(24, Math.floor(max * 0.55));
  const tail = max - head - 1;
  return `${url.slice(0, head)}…${url.slice(-tail)}`;
}

function filterBatchItems(items: BatchAddDownloadItem[], query: string): BatchAddDownloadItem[] {
  const needle = query.trim().toLowerCase();
  if (!needle) {
    return items;
  }
  return items.filter((item) => {
    const haystack = [
      item.filename,
      item.linkName,
      item.url,
      item.originalUrl,
      item.referer,
      item.fileType,
      item.streamKind,
      typeof item.fileSize === "number" ? String(item.fileSize) : "",
    ]
      .filter(Boolean)
      .join(" ")
      .toLowerCase();
    return haystack.includes(needle);
  });
}

export function BatchAddDownloadsPopupPage({ batchId }: BatchAddDownloadsPopupPageProps) {
  const { t } = useTranslation();
  const { formatBytes } = useUnitFormat();
  const client = useConnectionStore((s) => s.client);
  const connection = useConnectionStore((s) => s.connection);
  const queues = useDataStore((s) => s.queues);
  const defaultQueueId = useDataStore(selectEffectiveQueueId);

  const [loading, setLoading] = useState(true);
  const [payload, setPayload] = useState<BatchAddDownloadsPayload | null>(null);
  const [items, setItems] = useState<BatchAddDownloadItem[]>([]);
  const [selectedIds, setSelectedIds] = useState<string[]>([]);
  const [searchQuery, setSearchQuery] = useState("");
  const [queueId, setQueueId] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const selectionAnchorRef = useRef<string | null>(null);

  const filteredItems = useMemo(
    () => filterBatchItems(items, searchQuery),
    [items, searchQuery],
  );
  const visibleIds = useMemo(
    () => filteredItems.map((item) => item.id),
    [filteredItems],
  );

  const displayQueues = withDefaultQueue(
    queues.length > 0 ? queues : (payload?.queues ?? []),
    createDefaultQueueInfo(t("queue.defaultName"), t("queue.defaultDescription")),
  );

  const effectiveQueueId = queueId ?? payload?.defaultQueueId ?? defaultQueueId;
  const selectedQueue = selectSelectedQueue(displayQueues, effectiveQueueId);

  useEffect(() => {
    document.title = t("download.batchAdd.title");
  }, [t]);

  useEffect(() => {
    let cancelled = false;
    setLoading(true);

    void loadBatchAddPayload(batchId).then((next) => {
      if (cancelled) {
        return;
      }
      setPayload(next);
      const nextItems = next?.items ?? [];
      setItems(nextItems);
      setSelectedIds(nextItems.map((item) => item.id));
      const lastId = nextItems[nextItems.length - 1]?.id ?? null;
      selectionAnchorRef.current = lastId;
      setQueueId(next?.defaultQueueId ?? null);
      setLoading(false);
    });

    return () => {
      cancelled = true;
    };
  }, [batchId]);

  useEffect(() => {
    if (!client || connection !== "connected") {
      return;
    }
    void useDataStore.getState().refresh();
  }, [client, connection]);

  const columns = useMemo<DataTableColumn<BatchAddDownloadItem>[]>(
    () => [
      {
        id: "filename",
        header: t("download.filename"),
        width: 160,
        minWidth: 120,
        render: (row) => (
          <span className="avar-batch-add__cell-text" title={row.filename || row.url}>
            {row.filename || "—"}
          </span>
        ),
      },
      {
        id: "linkName",
        header: t("download.batchAdd.linkName"),
        width: 140,
        minWidth: 100,
        render: (row) => (
          <span className="avar-batch-add__cell-text" title={row.linkName || undefined}>
            {row.linkName || "—"}
          </span>
        ),
      },
      {
        id: "url",
        header: t("download.batchAdd.link"),
        width: 220,
        minWidth: 160,
        render: (row) => (
          <span className="avar-batch-add__cell-mono" title={row.url}>
            {truncateUrl(row.url)}
          </span>
        ),
      },
      {
        id: "fileType",
        header: t("download.batchAdd.fileType"),
        width: 88,
        minWidth: 72,
        render: (row) => formatBatchFileType(row.fileType, t),
      },
      {
        id: "originalUrl",
        header: t("download.batchAdd.originalUrl"),
        width: 180,
        minWidth: 120,
        render: (row) => {
          const value = row.originalUrl || row.referer || "";
          if (!value) {
            return "—";
          }
          return (
            <span className="avar-batch-add__cell-mono" title={value}>
              {truncateUrl(value, 56)}
            </span>
          );
        },
      },
      {
        id: "fileSize",
        header: t("download.batchAdd.fileSize"),
        width: 88,
        minWidth: 72,
        align: "end",
        render: (row) =>
          typeof row.fileSize === "number" && row.fileSize >= 0
            ? formatBytes(row.fileSize)
            : "—",
      },
    ],
    [formatBytes, t],
  );

  function applySelection(id: string, event?: MouseEvent) {
    const additive = Boolean(event?.ctrlKey || event?.metaKey);
    const range = Boolean(event?.shiftKey);
    const next = applyTableSelection({
      id,
      orderedIds: visibleIds,
      selectedIds,
      anchorId: selectionAnchorRef.current,
      additive,
      range,
    });
    setSelectedIds(next.selectedIds);
    selectionAnchorRef.current = next.anchorId;
  }

  function toggleSelect(id: string, event?: MouseEvent) {
    if (event?.shiftKey || event?.ctrlKey || event?.metaKey) {
      applySelection(id, event);
      return;
    }
    setSelectedIds((prev) => {
      const next = prev.includes(id) ? prev.filter((entry) => entry !== id) : [...prev, id];
      selectionAnchorRef.current = next.includes(id) ? id : (next[next.length - 1] ?? null);
      return next;
    });
  }

  function selectAll(checked: boolean) {
    if (!checked) {
      setSelectedIds([]);
      selectionAnchorRef.current = null;
      return;
    }
    setSelectedIds(visibleIds);
    selectionAnchorRef.current = visibleIds[visibleIds.length - 1] ?? null;
  }

  function deselectAll() {
    setSelectedIds([]);
    selectionAnchorRef.current = null;
  }

  function invertSelection() {
    const next = visibleIds.filter((id) => !selectedIds.includes(id));
    setSelectedIds(next);
    selectionAnchorRef.current = next[next.length - 1] ?? null;
  }

  function removeUnselected() {
    const keep = new Set(selectedIds);
    setItems((prev) => prev.filter((item) => keep.has(item.id)));
    setSearchQuery("");
  }

  async function handleSubmit(startQueueAfter: boolean) {
    if (!client || selectedIds.length === 0) {
      return;
    }

    const selectedItems = items.filter((item) => selectedIds.includes(item.id));
    const queueName = isDefaultQueue(effectiveQueueId) ? undefined : selectedQueue?.name;
    const usePerItemStart = startQueueAfter && isDefaultQueue(effectiveQueueId);

    setSubmitting(true);
    let succeeded = 0;
    let failed = 0;

    try {
      for (const item of selectedItems) {
        try {
          await client.addDownload({
            url: item.url,
            attached: false,
            startNow: usePerItemStart,
            queue: queueName,
            name: item.filename || item.linkName,
            referer: payload.pageUrl || item.referer || item.originalUrl,
            streamKind: item.streamKind || item.fileType,
          });
          succeeded += 1;
        } catch (err) {
          failed += 1;
          appLogger.gui.error(
            t("download.addFailed"),
            err instanceof Error ? err.message : undefined,
          );
        }
      }

      if (
        startQueueAfter &&
        !isDefaultQueue(effectiveQueueId) &&
        selectedQueue &&
        succeeded > 0
      ) {
        try {
          await client.startQueue(selectedQueue.id);
        } catch (err) {
          appLogger.gui.error(
            t("download.batchAdd.queueStartFailed"),
            err instanceof Error ? err.message : undefined,
          );
        }
      }

      if (failed === 0) {
        appLogger.gui.info(
          t("download.batchAdd.addSuccess", { count: succeeded }),
        );
        window.close();
      } else {
        appLogger.gui.warn(
          t("download.batchAdd.addPartial", { succeeded, failed }),
        );
      }

      void useDataStore.getState().refresh();
    } finally {
      setSubmitting(false);
    }
  }

  if (loading) {
    return (
      <div className="avar-popup-page avar-batch-add-popup">
        <Spinner />
      </div>
    );
  }

  if (!payload || items.length === 0) {
    return (
      <div className="avar-popup-page avar-batch-add-popup">
        <p className="avar-empty">{t("download.batchAdd.notFound")}</p>
      </div>
    );
  }

  return (
    <div className="avar-popup-page avar-batch-add-popup">
      <div className="avar-batch-add-popup__card">
        <header className="avar-batch-add-popup__header">
          <div className="avar-batch-add-popup__heading">
            <h1>{t("download.batchAdd.title")}</h1>
            {payload.pageTitle ? (
              <p className="avar-batch-add-popup__subtitle" title={payload.pageTitle}>
                {payload.pageTitle}
              </p>
            ) : null}
          </div>
          <div className="avar-batch-add-popup__queue">
            <Select
              compact
              label={t("download.targetQueue")}
              value={effectiveQueueId}
              onChange={(e) => setQueueId(e.target.value)}
            >
              {displayQueues.map((queue) => (
                <option key={queue.id} value={queue.id}>
                  {queue.name}
                </option>
              ))}
            </Select>
          </div>
        </header>

        <div className="avar-batch-add-popup__toolbar">
          <Input
            className="avar-batch-add-popup__search"
            type="search"
            value={searchQuery}
            placeholder={t("download.batchAdd.searchPlaceholder")}
            aria-label={t("download.batchAdd.searchLabel")}
            onChange={(e) => setSearchQuery(e.target.value)}
          />
          <div className="avar-batch-add-popup__toolbar-actions">
            <Button size="sm" variant="ghost" onClick={deselectAll} disabled={selectedIds.length === 0}>
              {t("download.batchAdd.deselectAll")}
            </Button>
            <Button
              size="sm"
              variant="ghost"
              onClick={invertSelection}
              disabled={filteredItems.length === 0}
            >
              {t("download.batchAdd.invertSelection")}
            </Button>
            <Button
              size="sm"
              variant="ghost"
              onClick={removeUnselected}
              disabled={selectedIds.length === 0 || selectedIds.length === items.length}
            >
              {t("download.batchAdd.removeUnselected")}
            </Button>
          </div>
          <p className="avar-batch-add-popup__selection-hint">{t("download.batchAdd.selectionHint")}</p>
        </div>

        <div className="avar-batch-add-popup__table">
          <DataTable
            className="avar-batch-add-popup__data-table"
            rows={filteredItems}
            columns={columns}
            getRowId={(row) => row.id}
            selectedIds={selectedIds}
            showCheckboxes
            selectAllLabel={t("download.batchAdd.selectAll")}
            onToggleSelect={toggleSelect}
            onSelectAll={selectAll}
            onRowClick={(row, event) => applySelection(row.id, event)}
            emptyMessage={
              searchQuery.trim() ? t("download.batchAdd.searchEmpty") : t("download.batchAdd.empty")
            }
            variant="flex"
            interactive
          />
        </div>

        <footer className="avar-batch-add-popup__footer">
          <span className="avar-batch-add-popup__count">
            {t("download.batchAdd.selectedCount", { count: selectedIds.length })}
          </span>
          <div className="avar-batch-add-popup__actions">
            <Button variant="ghost" onClick={() => window.close()}>
              {t("common.cancel")}
            </Button>
            <Button
              variant="secondary"
              loading={submitting}
              disabled={selectedIds.length === 0}
              onClick={() => void handleSubmit(false)}
            >
              <FontAwesomeIcon icon={faListCheck} />
              {t("download.queueSubmit")}
            </Button>
            <Button
              loading={submitting}
              disabled={selectedIds.length === 0}
              onClick={() => void handleSubmit(true)}
            >
              <FontAwesomeIcon icon={faPlay} />
              {t("download.batchAdd.queueAndStart")}
            </Button>
          </div>
        </footer>
      </div>
    </div>
  );
}
