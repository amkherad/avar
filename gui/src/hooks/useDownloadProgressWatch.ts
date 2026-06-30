import { useEffect } from "react";
import { useConnectionStore } from "@/stores/connectionStore";

/**
 * Tells the daemon to include segmented progress detail (chunkSize, doneRanges,
 * activeRanges) for this download while the detail view is mounted.
 */
export function useDownloadProgressWatch(downloadId: string | null | undefined): void {
  const connection = useConnectionStore((s) => s.connection);

  useEffect(() => {
    if (!downloadId || connection !== "connected") {
      return;
    }

    const client = useConnectionStore.getState().client;
    if (!client) {
      return;
    }

    void client.watchDownloadProgress(downloadId).catch(() => {
      // Non-fatal: bytesDownloaded still syncs; segment detail appears on next upsert.
    });

    return () => {
      void useConnectionStore
        .getState()
        .client?.unwatchDownloadProgress(downloadId)
        .catch(() => {});
    };
  }, [connection, downloadId]);
}
