import { useState } from "react";

import { useTranslation } from "react-i18next";

import { QueueList } from "@/components/queue/QueueList";

import { CreateQueueModal } from "@/components/queue/CreateQueueModal";

import { FontAwesomeIcon } from "@/icons";

import { faPlus, faSliders } from "@fortawesome/free-solid-svg-icons";

import { Button } from "@/components/ui/Button";

import { Spinner } from "@/components/ui/Spinner";

import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";

import { showConfirmDialog } from "@/lib/popup";

import { appLogger } from "@/lib/appLogger";

import { useConnectionStore } from "@/stores/connectionStore";

import {

  selectDownloadCounts,

  selectEffectiveQueueId,

  useDataStore,

} from "@/stores/dataStore";



export type QueuePanelMode = "select" | "manage";



export interface QueuePanelProps {

  mode?: QueuePanelMode;

  onManageQueues?: () => void;

  onModifyQueue?: (id: string) => void;

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

  const [busyId, setBusyId] = useState<string | null>(null);

  const [error, setError] = useState<string | null>(null);



  const isManage = mode === "manage";

  const showDelete = isManage;

  const selectable = !isManage;



  async function runQueueAction(id: string, action: () => Promise<void>) {

    if (!client || isDefaultQueue(id)) {

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



  return (

    <>

      <div className="avar-card__header" style={{ padding: 0, border: "none", marginBottom: "0.75rem" }}>

        <h3 className="avar-card__title">{t("queue.title")}</h3>

        <div className="avar-queue-panel__actions">

          {mode === "select" && onManageQueues ? (

            <Button size="sm" variant="ghost" onClick={onManageQueues}>

              <FontAwesomeIcon icon={faSliders} />

              {t("queue.manage")}

            </Button>

          ) : null}

          <Button size="sm" onClick={() => setModalOpen(true)}>

            <FontAwesomeIcon icon={faPlus} />

            {t("queue.add")}

          </Button>

        </div>

      </div>



      {error ? <p className="avar-field__error">{error}</p> : null}

      {status === "loading" && queues.length === 0 && downloads.length === 0 ? <Spinner /> : null}



      <QueueList

        queues={displayQueues}

        selectedId={selectable ? effectiveQueueId : null}

        downloadCounts={downloadCounts}

        showDelete={showDelete}

        selectable={selectable}

        showModify={mode === "select" && Boolean(onModifyQueue)}

        onSelect={setSelectedQueueId}

        onStart={(id) => void runQueueAction(id, () => client!.startQueue(id))}

        onStop={(id) => void runQueueAction(id, () => client!.stopQueue(id))}

        onModify={onModifyQueue}

        onDelete={async (id) => {

          const result = await showConfirmDialog({

            title: t("queue.delete"),

            message: t("queue.deleteConfirm"),

            confirmLabel: t("queue.delete"),

            cancelLabel: t("common.cancel"),

          });

          if (!result.confirmed) {

            appLogger.gui.debug("Queue delete cancelled", id);

            return;

          }

          void runQueueAction(id, () => client!.removeQueue(id, false));

        }}

        busyId={busyId}

      />



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

    </>

  );

}


