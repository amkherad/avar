import { useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import { QueueList } from "@/components/queue/QueueList";
import { QueueTable } from "@/components/queue/QueueTable";
import { QueueToolbar } from "@/components/queue/QueueToolbar";
import { CreateQueueModal } from "@/components/queue/CreateQueueModal";
import { EditQueueModal } from "@/components/queue/EditQueueModal";
import { FontAwesomeIcon } from "@/icons";
import { faPlus, faSliders } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { Spinner } from "@/components/ui/Spinner";
import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";
import { showConfirmDialog } from "@/lib/popup";
import { appLogger } from "@/lib/appLogger";
import {
  filterQueuesBySearch,
  filterQueuesByStatus,
  sortQueues,
  type QueueSort,
  type QueueStatusFilter,
} from "@/lib/queueFilterSort";
import { useConnectionStore } from "@/stores/connectionStore";
import {
  selectDownloadCounts,
  selectEffectiveQueueId,
  useDataStore,
} from "@/stores/dataStore";
import type { QueueInfo } from "@/api/types";

export type QueuePanelMode = "select" | "manage";

export interface QueuePanelProps {
  mode?: QueuePanelMode;
  onManageQueues?: () => void;
  onModifyQueue?: (id: string) => void;
}

function actionableQueue(queue: QueueInfo): boolean {
  return !queue.readonly && !isDefaultQueue(queue.id);
}

export function QueuePanel({ mode = "select", onManageQueues, onModifyQueue }: QueuePanelProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const queues = useDataStore((s) => s.queues);
  const downloads = useDataStore((s) => s.downloads);
  const status = useDataStore((s) => s.status);
  const setSelectedQueueId = useDataStore((s) => s.setSelectedQueueId);

  const effectiveQueueId = useDataStore(selectEffectiveQueueId);
  const defaultQueue = createDefaultQueueInfo(
    t("queue.defaultName"),
    t("queue.defaultDescription"),
  );
  const displayQueues = withDefaultQueue(queues, defaultQueue);
  const downloadCounts = selectDownloadCounts(displayQueues, downloads);

  const [modalOpen, setModalOpen] = useState(false);
  const [editQueueId, setEditQueueId] = useState<string | null>(null);
  const [busyId, setBusyId] = useState<string | null>(null);
  const [batchBusy, setBatchBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState("");
  const [statusFilter, setStatusFilter] = useState<QueueStatusFilter>("all");
  const [sort, setSort] = useState<QueueSort>({ key: null, direction: "asc" });
  const [selectedQueueIds, setSelectedQueueIds] = useState<string[]>([]);
  const [showCheckboxes, setShowCheckboxes] = useState(true);

  const isManage = mode === "manage";
  const showDelete = isManage;
  const showModify = isManage || Boolean(onModifyQueue);
  const selectable = !isManage;

  const filteredQueues = useMemo(() => {
    const searched = filterQueuesBySearch(displayQueues, searchQuery, downloadCounts);
    const statusFiltered = filterQueuesByStatus(searched, statusFilter);
    return sortQueues(statusFiltered, sort, downloadCounts);
  }, [displayQueues, searchQuery, statusFilter, sort, downloadCounts]);

  const selectedQueues = useMemo(
    () => displayQueues.filter((queue) => selectedQueueIds.includes(queue.id)),
    [displayQueues, selectedQueueIds],
  );

  const editQueue = useMemo(
    () => (editQueueId ? displayQueues.find((queue) => queue.id === editQueueId) ?? null : null),
    [displayQueues, editQueueId],
  );

  function handleModify(id: string) {
    if (onModifyQueue) {
      onModifyQueue(id);
      return;
    }
    setEditQueueId(id);
  }

  async function runQueueAction(id: string, action: () => Promise<void>) {
    if (!client || isDefaultQueue(id)) {
      return;
    }
    const queue = displayQueues.find((item) => item.id === id);
    if (!queue || queue.readonly) {
      return;
    }
    setBusyId(id);
    setError(null);
    try {
      await action();
      appLogger.gui.info("Queue action completed", id);
      await useDataStore.getState().refresh();
    } catch (err) {
      const message = err instanceof Error ? err.message : t("common.error");
      setError(message);
      appLogger.gui.error("Queue action failed", message);
    } finally {
      setBusyId(null);
    }
  }

  async function runBatchQueueAction(ids: string[], action: (id: string) => Promise<void>) {
    if (!client) {
      return;
    }
    const targets = ids.filter((id) => {
      const queue = displayQueues.find((item) => item.id === id);
      return queue ? actionableQueue(queue) : false;
    });
    if (targets.length === 0) {
      return;
    }
    setBatchBusy(true);
    setError(null);
    try {
      for (const id of targets) {
        await action(id);
      }
      appLogger.gui.info("Batch queue action completed", targets.join(", "));
      await useDataStore.getState().refresh();
      setSelectedQueueIds([]);
    } catch (err) {
      const message = err instanceof Error ? err.message : t("common.error");
      setError(message);
      appLogger.gui.error("Batch queue action failed", message);
    } finally {
      setBatchBusy(false);
    }
  }

  async function confirmDelete(ids: string[]) {
    const message =
      ids.length > 1
        ? t("queue.deleteConfirmBatch", { count: ids.length })
        : t("queue.deleteConfirm");
    const result = await showConfirmDialog({
      title: t("queue.delete"),
      message,
      confirmLabel: t("queue.delete"),
      cancelLabel: t("common.cancel"),
    });
    if (!result.confirmed) {
      appLogger.gui.debug("Queue delete cancelled", ids.join(", "));
      return;
    }
    if (ids.length === 1) {
      void runQueueAction(ids[0], () => client!.removeQueue(ids[0], false));
      return;
    }
    void runBatchQueueAction(ids, (id) => client!.removeQueue(id, false));
  }

  function handleToggleSelect(queueId: string) {
    setSelectedQueueIds((current) =>
      current.includes(queueId)
        ? current.filter((id) => id !== queueId)
        : [...current, queueId],
    );
  }

  function handleSelectAll(checked: boolean) {
    if (!checked) {
      setSelectedQueueIds([]);
      return;
    }
    setSelectedQueueIds(
      filteredQueues.filter(actionableQueue).map((queue) => queue.id),
    );
  }

  function handleContextMenu(queueId: string) {
    if (!selectedQueueIds.includes(queueId)) {
      setSelectedQueueIds([queueId]);
    }
  }

  const emptyMessage =
    searchQuery || statusFilter !== "all" ? t("queue.searchEmpty") : t("queue.empty");

  return (
    <>
      <div
        className="avar-card__header"
        style={{ padding: 0, border: "none", marginBottom: "0.75rem" }}
      >
        <h3 className="avar-card__title">{t("queue.title")}</h3>
        <div className="avar-queue-panel__actions">
          {mode === "select" && onManageQueues ? (
            <Button
              size="sm"
              variant="ghost"
              aria-label={t("queue.manage")}
              title={t("queue.manage")}
              onClick={onManageQueues}
            >
              <FontAwesomeIcon icon={faSliders} />
            </Button>
          ) : null}
          <Button
            size="sm"
            aria-label={t("queue.add")}
            title={t("queue.add")}
            onClick={() => setModalOpen(true)}
          >
            <FontAwesomeIcon icon={faPlus} />
          </Button>
        </div>
      </div>

      {error ? <p className="avar-field__error">{error}</p> : null}

      {isManage ? (
        <>
          <QueueToolbar
            searchQuery={searchQuery}
            onSearchChange={setSearchQuery}
            selectedQueues={selectedQueues}
            showCheckboxes={showCheckboxes}
            onToggleCheckboxes={() => setShowCheckboxes((value) => !value)}
            batchBusy={batchBusy}
            onBatchStart={(ids) =>
              void runBatchQueueAction(ids, (id) => client!.startQueue(id))
            }
            onBatchStop={(ids) => void runBatchQueueAction(ids, (id) => client!.stopQueue(id))}
            onBatchDelete={(ids) => void confirmDelete(ids)}
          />
          <QueueTable
            queues={filteredQueues}
            downloadCounts={downloadCounts}
            selectedIds={selectedQueueIds}
            showCheckboxes={showCheckboxes}
            showDelete={showDelete}
            showModify={showModify}
            onStart={(id) => void runQueueAction(id, () => client!.startQueue(id))}
            onStop={(id) => void runQueueAction(id, () => client!.stopQueue(id))}
            onModify={handleModify}
            onDelete={(id) => void confirmDelete([id])}
            onToggleSelect={handleToggleSelect}
            onSelectAll={handleSelectAll}
            onContextMenu={handleContextMenu}
            busyId={busyId}
            batchBusy={batchBusy}
            loading={status === "loading" && queues.length === 0 && downloads.length === 0}
            emptyMessage={emptyMessage}
            statusFilter={statusFilter}
            onStatusFilterChange={setStatusFilter}
            sort={sort}
            onSortChange={setSort}
          />
        </>
      ) : (
        <>
          {status === "loading" && queues.length === 0 && downloads.length === 0 ? (
            <Spinner />
          ) : null}
          <QueueList
            queues={displayQueues}
            selectedId={selectable ? effectiveQueueId : null}
            downloadCounts={downloadCounts}
            compact
            showDelete={showDelete}
            selectable={selectable}
            showModify={mode === "select" && Boolean(onModifyQueue)}
            onSelect={setSelectedQueueId}
            onStart={(id) => void runQueueAction(id, () => client!.startQueue(id))}
            onStop={(id) => void runQueueAction(id, () => client!.stopQueue(id))}
            onModify={onModifyQueue}
            onDelete={(id) => void confirmDelete([id])}
            busyId={busyId}
          />
        </>
      )}

      <CreateQueueModal
        open={modalOpen}
        onClose={() => setModalOpen(false)}
        onCreated={(id) => {
          if (selectable) {
            setSelectedQueueId(id);
          }
          void useDataStore.getState().refresh();
        }}
      />

      <EditQueueModal
        queue={editQueue}
        open={editQueueId !== null}
        onClose={() => setEditQueueId(null)}
        onSaved={() => void useDataStore.getState().refresh()}
      />
    </>
  );
}
