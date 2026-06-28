import { useCallback, useEffect, useState } from "react";
import type { DownloadDetails } from "@/api/types";
import { useConnectionStore } from "@/stores/connectionStore";

export function useDownloadDetails(downloadId: string | null) {
  const client = useConnectionStore((s) => s.client);
  const [details, setDetails] = useState<DownloadDetails | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    if (!client || !downloadId) {
      setDetails(null);
      setError(null);
      return;
    }

    setLoading(true);
    setError(null);
    try {
      const next = await client.getDownloadDetails(downloadId);
      setDetails(next);
    } catch (err) {
      setDetails(null);
      setError(err instanceof Error ? err.message : "Failed to load download details");
    } finally {
      setLoading(false);
    }
  }, [client, downloadId]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  return { details, loading, error, refresh };
}
