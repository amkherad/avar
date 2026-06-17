import { useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faArrowDown } from "@fortawesome/free-solid-svg-icons";
import { Modal } from "@/components/ui/Modal";
import { Input } from "@/components/ui/Input";
import { Button } from "@/components/ui/Button";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore, selectEffectiveQueueId, selectSelectedQueue } from "@/stores/dataStore";
import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";
import { appLogger } from "@/lib/appLogger";
import { useShortcutAction } from "@/shortcuts/useShortcutAction";

export interface AddDownloadModalProps {
  open: boolean;
  onClose: () => void;
}

export function AddDownloadModal({ open, onClose }: AddDownloadModalProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const queues = useDataStore((s) => s.queues);
  const queueId = useDataStore(selectEffectiveQueueId);
  const [url, setUrl] = useState("");
  const [adding, setAdding] = useState(false);

  const displayQueues = withDefaultQueue(
    queues,
    createDefaultQueueInfo(t("queue.defaultName"), t("queue.defaultDescription")),
  );
  const selectedQueue = selectSelectedQueue(displayQueues, queueId);

  async function handleSubmit() {
    if (!client || !url.trim()) {
      return;
    }
    setAdding(true);
    try {
      await client.addDownload(
        url.trim(),
        isDefaultQueue(queueId) ? undefined : selectedQueue?.name,
      );
      appLogger.gui.info(t("download.added", { url: url.trim() }));
      setUrl("");
      onClose();
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

  return (
    <Modal
      open={open}
      title={t("download.add")}
      onClose={onClose}
      footer={
        <Button loading={adding} onClick={() => void handleSubmit()}>
          <FontAwesomeIcon icon={faArrowDown} />
          {t("download.submit")}
        </Button>
      }
    >
      <Input
        label={t("download.url")}
        value={url}
        onChange={(e) => setUrl(e.target.value)}
        placeholder="https://"
        autoFocus
        onKeyDown={(e) => {
          if (e.key === "Enter") {
            void handleSubmit();
          }
        }}
      />
    </Modal>
  );
}

export function useAddDownloadModal() {
  const [open, setOpen] = useState(false);

  return {
    open,
    openModal: () => setOpen(true),
    closeModal: () => setOpen(false),
  };
}
