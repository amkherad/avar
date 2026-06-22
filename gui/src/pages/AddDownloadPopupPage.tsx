import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faListCheck, faPlay } from "@fortawesome/free-solid-svg-icons";
import type { AddDownloadParams } from "@/api/daemon";
import { loadAddDownloadPrefill, type AddDownloadPrefill } from "@/lib/addDownloadPrefill";
import { DOWNLOAD_GROUPS } from "@/lib/downloadGroups";
import { appLogger } from "@/lib/appLogger";
import { defaultProxySettings } from "@/lib/proxySettings";
import { ProxySettingsFields } from "@/components/settings/ProxySettingsFields";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore, selectEffectiveQueueId, selectSelectedQueue } from "@/stores/dataStore";
import { createDefaultQueueInfo, isDefaultQueue, withDefaultQueue } from "@/queue/defaultQueue";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Select } from "@/components/ui/Select";
import { Spinner } from "@/components/ui/Spinner";

export interface AddDownloadPopupPageProps {
  addId: string;
}

export function AddDownloadPopupPage({ addId }: AddDownloadPopupPageProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const queues = useDataStore((s) => s.queues);
  const defaultQueueId = useDataStore(selectEffectiveQueueId);

  const [loading, setLoading] = useState(true);
  const [prefill, setPrefill] = useState<AddDownloadPrefill | null>(null);
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

  const effectiveQueueId = queueId ?? prefill?.defaultQueueId ?? defaultQueueId;
  const selectedQueue = selectSelectedQueue(displayQueues, effectiveQueueId);

  useEffect(() => {
    document.title = t("download.add");
  }, [t]);

  useEffect(() => {
    let cancelled = false;
    setLoading(true);

    void loadAddDownloadPrefill(addId).then((next) => {
      if (cancelled) {
        return;
      }
      setPrefill(next);
      if (next) {
        setUrl(next.url);
        setFilename(next.filename ?? "");
        setQueueId(next.defaultQueueId ?? null);
      }
      setLoading(false);
    });

    return () => {
      cancelled = true;
    };
  }, [addId]);

  useEffect(() => {
    if (!client) {
      return;
    }
    void useDataStore.getState().refresh();
  }, [client]);

  async function handleSubmit(startNow: boolean) {
    if (!client || !url.trim()) {
      return;
    }

    const params: AddDownloadParams = {
      url: url.trim(),
      attached: false,
      startNow,
      queue: isDefaultQueue(effectiveQueueId) ? undefined : selectedQueue?.name,
      name: filename.trim() || undefined,
      outputPath: outputPath.trim() || undefined,
      group: group !== "default" ? group : undefined,
      proxy,
      referer: prefill?.referer,
      streamKind: prefill?.streamKind,
    };

    setAdding(true);
    try {
      await client.addDownload(params);
      appLogger.gui.info(
        startNow ? t("download.started", { url: url.trim() }) : t("download.queued", { url: url.trim() }),
      );
      window.close();
      void useDataStore.getState().refresh();
    } catch (err) {
      appLogger.gui.error(
        t("download.addFailed"),
        err instanceof Error ? err.message : undefined,
      );
    } finally {
      setAdding(false);
    }
  }

  if (loading) {
    return (
      <div className="avar-popup-page avar-add-download-popup">
        <Spinner />
      </div>
    );
  }

  if (!prefill) {
    return (
      <div className="avar-popup-page avar-add-download-popup">
        <p className="avar-empty">{t("download.addPrefillNotFound")}</p>
      </div>
    );
  }

  return (
    <div className="avar-popup-page avar-add-download-popup">
      <div className="avar-add-download-popup__card">
        <header className="avar-add-download-popup__header">
          <div className="avar-add-download-popup__heading">
            <h1>{t("download.add")}</h1>
            {prefill.pageTitle ? (
              <p className="avar-add-download-popup__subtitle" title={prefill.pageTitle}>
                {prefill.pageTitle}
              </p>
            ) : null}
          </div>
        </header>

        <div className="avar-add-download-popup__form">
          <Input
            label={t("download.url")}
            value={url}
            onChange={(e) => setUrl(e.target.value)}
            placeholder="https://"
            autoFocus
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
        </div>

        <footer className="avar-add-download-popup__footer">
          <Button variant="ghost" onClick={() => window.close()}>
            {t("common.cancel")}
          </Button>
          <div className="avar-add-download-popup__actions">
            <Button loading={adding} variant="secondary" onClick={() => void handleSubmit(false)}>
              <FontAwesomeIcon icon={faListCheck} />
              {t("download.queueSubmit")}
            </Button>
            <Button loading={adding} onClick={() => void handleSubmit(true)}>
              <FontAwesomeIcon icon={faPlay} />
              {t("download.startNow")}
            </Button>
          </div>
        </footer>
      </div>
    </div>
  );
}
