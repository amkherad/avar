import { useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faListCheck, faPlay } from "@fortawesome/free-solid-svg-icons";
import type { BatchAddDownloadItem } from "@/lib/batchAddDownloads";
import {
  formatBatchFileType,
  loadBatchAddPayload,
  type BatchAddDownloadsPayload,
} from "@/lib/batchAddDownloads";
import { formatBytes } from "@/lib/formatBytes";
import { appLogger } from "@/lib/appLogger";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore, selectEffectiveQueueId, selectSelectedQueue } from "@/stores/dataStore";
import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";
import { Button } from "@/components/ui/Button";
import { DataTable, type DataTableColumn } from "@/components/ui/DataTable";
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

export function BatchAddDownloadsPopupPage({ batchId }: BatchAddDownloadsPopupPageProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const queues = useDataStore((s) => s.queues);
  const defaultQueueId = useDataStore(selectEffectiveQueueId);

  const [loading, setLoading] = useState(true);
  const [payload, setPayload] = useState<BatchAddDownloadsPayload | null>(null);
  const [items, setItems] = useState<BatchAddDownloadItem[]>([]);
  const [selectedIds, setSelectedIds] = useState<string[]>([]);
  const [queueId, setQueueId] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);

  const displayQueues = withDefaultQueue(
    queues,
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
      setQueueId(next?.defaultQueueId ?? null);
      setLoading(false);
    });

    return () => {
      cancelled = true;
    };
  }, [batchId]);

  useEffect(() => {
    if (!client) {
      return;
    }
    void useDataStore.getState().refresh();
  }, [client]);

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
    [t],
  );

  function toggleSelect(id: string) {
    setSelectedIds((prev) =>
      prev.includes(id) ? prev.filter((entry) => entry !== id) : [...prev, id],
    );
  }

  function selectAll(checked: boolean) {
    setSelectedIds(checked ? items.map((item) => item.id) : []);
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
            referer: item.referer || item.originalUrl,
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

        <div className="avar-batch-add-popup__table">
          <DataTable
            rows={items}
            columns={columns}
            getRowId={(row) => row.id}
            selectedIds={selectedIds}
            showCheckboxes
            selectAllLabel={t("download.batchAdd.selectAll")}
            onToggleSelect={toggleSelect}
            onSelectAll={selectAll}
            emptyMessage={t("download.batchAdd.empty")}
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
