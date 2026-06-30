import { useCallback, useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { useTranslation } from "react-i18next";
import type { DownloadInfo } from "@/api/types";
import { Modal } from "@/components/ui/Modal";
import { Button } from "@/components/ui/Button";
import type { DaemonClient } from "@/api/daemon";
import type { DownloadDetails } from "@/api/types";
import { downloadRefererUrl, referrersLikelyMatch } from "@/lib/downloadLinkValidation";
import { checkDownloadSizeCompatibility } from "@/lib/downloadSizeCheck";
import {
  cancelLinkRefreshSession,
  pollLinkRefreshSession,
  startLinkRefreshSession,
  type LinkRefreshCapturedPayload,
} from "@/lib/linkRefreshSession";
import { openExternalUrl } from "@/lib/openExternalUrl";
import { copyTextToClipboard } from "@/lib/curlCommand";
import { showConfirmDialog } from "@/lib/popup";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";
import { appLogger } from "@/lib/appLogger";

export interface RefreshLinkModalProps {
  download: DownloadInfo;
  details?: DownloadDetails;
  onClose: () => void;
}

async function applyRefreshedLink(
  client: DaemonClient,
  download: DownloadInfo,
  details: DownloadDetails | undefined,
  captured: LinkRefreshCapturedPayload,
  t: (key: string) => string,
): Promise<boolean> {
  const referer = captured.referer;
  const existingReferer = downloadRefererUrl(details, download);

  if (!referrersLikelyMatch(existingReferer, referer)) {
    const confirmed = await showConfirmDialog({
      title: t("download.refererMismatchTitle"),
      message: t("download.refererMismatchMessage"),
      confirmLabel: t("download.refererMismatchUse"),
      cancelLabel: t("common.cancel"),
    });
    if (!confirmed.confirmed) {
      appLogger.gui.debug("Referer mismatch rejected", download.id);
      return false;
    }
  }

  const sizeCheck = await checkDownloadSizeCompatibility(
    client,
    download,
    details,
    captured.url,
    referer,
  );
  if (!sizeCheck.ok && sizeCheck.reason === "mismatch") {
    if (sizeCheck.choice === "restart") {
      await client.setDownloadSource(download.id, {
        url: captured.url,
        referer,
        originalPage: referer,
      });
      await client.restartDownload(download.id);
      await useDataStore.getState().refresh();
      return true;
    }
    if (sizeCheck.choice === "new") {
      await client.addDownload({
        url: captured.url,
        queue: download.queueId,
        name: download.filename,
        referer,
        startNow: true,
        forceNew: true,
      });
      await useDataStore.getState().refresh();
      return true;
    }
    return false;
  }

  await client.setDownloadSource(download.id, {
    url: captured.url,
    referer,
    originalPage: referer,
  });
  await useDataStore.getState().refresh();
  appLogger.gui.info("Download link refreshed", download.filename);
  return true;
}

export function RefreshLinkModal({ download, details, onClose }: RefreshLinkModalProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [sessionId, setSessionId] = useState<string | null>(null);
  const [waiting, setWaiting] = useState(true);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [refererCopied, setRefererCopied] = useState(false);
  const cancelledRef = useRef(false);
  const refererOpenedRef = useRef(false);
  const refererUrl = downloadRefererUrl(details, download);

  const cleanup = useCallback(async () => {
    cancelledRef.current = true;
    if (sessionId) {
      await cancelLinkRefreshSession(sessionId);
    }
  }, [sessionId]);

  useEffect(() => {
    cancelledRef.current = false;
    let activeSessionId: string | null = null;

    async function begin() {
      setError(null);
      setWaiting(true);
      try {
        activeSessionId = await startLinkRefreshSession(download.id);
        if (cancelledRef.current) {
          await cancelLinkRefreshSession(activeSessionId);
          return;
        }
        setSessionId(activeSessionId);
        if (refererUrl && !refererOpenedRef.current) {
          refererOpenedRef.current = true;
          void openExternalUrl(refererUrl);
        }
      } catch (err) {
        setError(err instanceof Error ? err.message : t("download.refreshLinkStartFailed"));
        setWaiting(false);
      }
    }

    void begin();

    return () => {
      cancelledRef.current = true;
      if (activeSessionId) {
        void cancelLinkRefreshSession(activeSessionId);
      }
    };
  }, [download.id, refererUrl, t]);

  useEffect(() => {
    if (!sessionId || !waiting || !client) {
      return;
    }

    const controller = new AbortController();
    let timer: number | undefined;

    async function poll() {
      try {
        const captured = await pollLinkRefreshSession(sessionId!, controller.signal);
        if (cancelledRef.current || !captured) {
          if (!cancelledRef.current) {
            timer = window.setTimeout(() => void poll(), 750);
          }
          return;
        }

        setWaiting(false);
        setBusy(true);
        try {
          if (!client) {
            return;
          }
          const applied = await applyRefreshedLink(client, download, details, captured, t);
          if (applied) {
            await cancelLinkRefreshSession(sessionId!);
            onClose();
          } else {
            await cancelLinkRefreshSession(sessionId!);
            const nextSessionId = await startLinkRefreshSession(download.id);
            setSessionId(nextSessionId);
            setWaiting(true);
          }
        } catch (err) {
          setError(err instanceof Error ? err.message : t("download.refreshLinkApplyFailed"));
          setWaiting(false);
        } finally {
          setBusy(false);
        }
      } catch {
        if (!cancelledRef.current) {
          timer = window.setTimeout(() => void poll(), 1500);
        }
      }
    }

    void poll();

    return () => {
      controller.abort();
      if (timer !== undefined) {
        window.clearTimeout(timer);
      }
    };
  }, [client, details, download, onClose, sessionId, t, waiting]);

  async function handleCancel() {
    appLogger.gui.debug("Link refresh cancelled", download.id);
    await cleanup();
    onClose();
  }

  function handleOpenRefererPage() {
    if (!refererUrl) {
      return;
    }
    void openExternalUrl(refererUrl);
  }

  async function handleCopyRefererPage() {
    if (!refererUrl) {
      return;
    }
    const ok = await copyTextToClipboard(refererUrl);
    if (ok) {
      setRefererCopied(true);
      window.setTimeout(() => setRefererCopied(false), 1500);
    }
  }

  return createPortal(
    <Modal
      open
      title={t("download.refreshLinkTitle")}
      onClose={() => void handleCancel()}
      footer={
        <Button variant="ghost" onClick={() => void handleCancel()} disabled={busy}>
          {t("common.cancel")}
        </Button>
      }
    >
      {error ? (
        <p className="avar-download-detail__error">{error}</p>
      ) : waiting ? (
        <p>{t("download.refreshLinkWaiting", { name: download.filename })}</p>
      ) : (
        <p>{t("download.refreshLinkProcessing")}</p>
      )}
      {refererUrl ? (
        <div className="avar-refresh-link__actions">
          <button
            type="button"
            className="avar-refresh-link__page-link"
            onClick={handleOpenRefererPage}
            disabled={busy}
          >
            {t("download.refreshLinkOpenPage")}
          </button>
          <button
            type="button"
            className="avar-refresh-link__page-link"
            onClick={() => void handleCopyRefererPage()}
            disabled={busy}
          >
            {refererCopied ? t("common.copied") : t("download.refreshLinkCopyPage")}
          </button>
        </div>
      ) : null}
    </Modal>,
    document.body,
  );
}
