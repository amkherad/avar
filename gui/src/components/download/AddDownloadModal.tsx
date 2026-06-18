import { useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faListCheck } from "@fortawesome/free-solid-svg-icons";
import { Modal } from "@/components/ui/Modal";
import { Input } from "@/components/ui/Input";
import { Select } from "@/components/ui/Select";
import { Button } from "@/components/ui/Button";
import { ProxySettingsFields } from "@/components/settings/ProxySettingsFields";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore, selectEffectiveQueueId, selectSelectedQueue } from "@/stores/dataStore";
import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";
import { DOWNLOAD_GROUPS } from "@/lib/downloadGroups";
import { defaultProxySettings } from "@/lib/proxySettings";
import { appLogger } from "@/lib/appLogger";
import type { AddDownloadParams } from "@/api/daemon";

export interface AddDownloadModalProps {
  open: boolean;
  onClose: () => void;
}

export function AddDownloadModal({ open, onClose }: AddDownloadModalProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const queues = useDataStore((s) => s.queues);
  const defaultQueueId = useDataStore(selectEffectiveQueueId);

  const [url, setUrl] = useState("");
  const [queueId, setQueueId] = useState<string | null>(null);
  const [filename, setFilename] = useState("");
  const [outputPath, setOutputPath] = useState("");
  const [group, setGroup] = useState<string>("default");
  const [proxy, setProxy] = useState(defaultProxySettings);
  const [adding, setAdding] = useState(false);

  const displayQueues = withDefaultQueue(
    queues,
    createDefaultQueueInfo(t("queue.defaultName"), t("queue.defaultDescription")),
  );

  const effectiveQueueId = queueId ?? defaultQueueId;
  const selectedQueue = selectSelectedQueue(displayQueues, effectiveQueueId);

  function resetForm() {
    setUrl("");
    setQueueId(null);
    setFilename("");
    setOutputPath("");
    setGroup("default");
    setProxy(defaultProxySettings());
  }

  async function handleSubmit() {
    if (!client || !url.trim()) {
      return;
    }

    const params: AddDownloadParams = {
      url: url.trim(),
      attached: true,
      queue: isDefaultQueue(effectiveQueueId) ? undefined : selectedQueue?.name,
      name: filename.trim() || undefined,
      outputPath: outputPath.trim() || undefined,
      group: group !== "default" ? group : undefined,
      proxy,
    };

    setAdding(true);
    try {
      await client.addDownload(params);
      appLogger.gui.info(t("download.queued", { url: url.trim() }));
      resetForm();
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
      wide
      footer={
        <>
          <Button variant="secondary" onClick={onClose}>
            {t("common.cancel")}
          </Button>
          <Button loading={adding} onClick={() => void handleSubmit()}>
            <FontAwesomeIcon icon={faListCheck} />
            {t("download.queueSubmit")}
          </Button>
        </>
      }
    >
      <Input
        label={t("download.url")}
        value={url}
        onChange={(e) => setUrl(e.target.value)}
        placeholder="https://"
        autoFocus
        onKeyDown={(e) => {
          if (e.key === "Enter" && !e.shiftKey) {
            void handleSubmit();
          }
        }}
      />

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

      <Input
        label={t("download.filename")}
        value={filename}
        onChange={(e) => setFilename(e.target.value)}
        hint={t("download.filenameHint")}
      />

      <Input
        label={t("download.outputPath")}
        value={outputPath}
        onChange={(e) => setOutputPath(e.target.value)}
        hint={t("download.outputPathHint")}
      />

      <Select
        label={t("download.group")}
        value={group}
        onChange={(e) => setGroup(e.target.value)}
      >
        {DOWNLOAD_GROUPS.map((id) => (
          <option key={id} value={id}>
            {t(`download.groups.${id}`)}
          </option>
        ))}
      </Select>

      <ProxySettingsFields value={proxy} onChange={setProxy} />
    </Modal>
  );
}

export function useAddDownloadModal(
  controlledOpen?: boolean,
  onOpenChange?: (open: boolean) => void,
) {
  const [uncontrolledOpen, setUncontrolledOpen] = useState(false);
  const open = controlledOpen ?? uncontrolledOpen;

  function setOpen(next: boolean) {
    if (controlledOpen === undefined) {
      setUncontrolledOpen(next);
    }
    onOpenChange?.(next);
  }

  return {
    open,
    openModal: () => setOpen(true),
    closeModal: () => setOpen(false),
  };
}
