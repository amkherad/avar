import { useEffect } from "react";
import { useConnectionStore } from "@/stores/connectionStore";

/**
 * Tells the daemon to include segmented progress detail (chunkSize, doneRanges)
 * for this download while the detail view is mounted.
 */
export function useDownloadProgressWatch(downloadId: string | null | undefined): void {
  useEffect(() => {
    if (!downloadId) {
      return;
    }

    const client = useConnectionStore.getState().client;
    if (!client) {
      return;
    }

    void client.watchDownloadProgress(downloadId);

    return () => {
      void useConnectionStore.getState().client?.unwatchDownloadProgress(downloadId);
    };
  }, [downloadId]);
}
