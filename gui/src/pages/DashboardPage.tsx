import { useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Spinner } from "@/components/ui/Spinner";
import { DownloadRow } from "@/components/download/DownloadList";
import { useConnectionStore } from "@/stores/connectionStore";
import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";
import {
  selectDownloadsForQueue,
  selectEffectiveQueueId,
  selectSelectedQueue,
  useDataStore,
} from "@/stores/dataStore";
import { restartDataSync } from "@/sync/syncManager";

function DownloadPanel() {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const queues = useDataStore((s) => s.queues);
  const downloads = useDataStore((s) => s.downloads);
  const status = useDataStore((s) => s.status);
  const queueId = useDataStore(selectEffectiveQueueId);
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

  const [addUrl, setAddUrl] = useState("");
  const [adding, setAdding] = useState(false);

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
      await useDataStore.getState().refresh();
    } finally {
      setAdding(false);
    }
  }

  return (
    <Card
      title={
        selectedQueue
          ? `${t("download.title")} — ${selectedQueue.name}`
          : t("download.title")
      }
    >
      <div style={{ display: "flex", gap: "0.5rem", marginBottom: "1rem" }}>
        <Input
          label={t("download.url")}
          value={addUrl}
          onChange={(e) => setAddUrl(e.target.value)}
          placeholder="https://"
        />
        <div style={{ alignSelf: "flex-end" }}>
          <Button loading={adding} onClick={() => void handleAddDownload()}>
            {t("download.submit")}
          </Button>
        </div>
      </div>

      {status === "loading" && queueDownloads.length === 0 ? <Spinner /> : null}
      {status !== "loading" && queueDownloads.length === 0 ? (
        <p className="avar-empty">{t("download.empty")}</p>
      ) : null}
      {queueDownloads.map((item) => (
        <DownloadRow key={item.id || item.filename} download={item} />
      ))}
    </Card>
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
