import { useCallback, useState } from "react";
import type { DownloadDetails, DownloadInfo } from "@/api/types";
import { RefreshLinkModal } from "@/components/download/RefreshLinkModal";
import { appLogger } from "@/lib/appLogger";
import { useConnectionStore } from "@/stores/connectionStore";

interface RefreshLinkContext {
  download: DownloadInfo;
  details?: DownloadDetails;
}

function seedDetailsFromDownload(download: DownloadInfo): DownloadDetails | undefined {
  const url = download.url;
  const referer = download.referer;
  if (!url && !referer) {
    return undefined;
  }
  return { url, referer };
}

export function useRefreshLinkModal() {
  const client = useConnectionStore((s) => s.client);
  const [context, setContext] = useState<RefreshLinkContext | null>(null);
  const [loading, setLoading] = useState(false);

  const openRefreshLink = useCallback(
    (download: DownloadInfo) => {
      if (!client) {
        return;
      }

      appLogger.gui.debug("Link refresh opened", download.id);
      setContext({ download, details: seedDetailsFromDownload(download) });

      void (async () => {
        setLoading(true);
        try {
          const details = await client.getDownloadDetails(download.id);
          setContext((current) =>
            current?.download.id === download.id ? { download, details } : current,
          );
        } catch {
          // Keep seeded details so the waiting modal stays open.
        } finally {
          setLoading(false);
        }
      })();
    },
    [client],
  );
  const closeRefreshLink = useCallback(() => {
    setContext(null);
  }, []);

  const refreshLinkModal = context ? (
    <RefreshLinkModal
      download={context.download}
      details={context.details}
      onClose={closeRefreshLink}
    />
  ) : null;

  return {
    openRefreshLink,
    closeRefreshLink,
    refreshLinkModal,
    refreshLinkLoading: loading,
  };
}
