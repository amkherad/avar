import { create } from "zustand";
import type { DownloadInfo, HealthInfo, QueueInfo } from "@/api/types";
import { DEFAULT_QUEUE_ID, isDefaultQueue } from "@/queue/defaultQueue";
import { useConnectionStore } from "@/stores/connectionStore";
import { appLogger } from "@/lib/appLogger";

export type DataStatus = "idle" | "loading" | "error";

export interface SnapshotPayload {
  type?: string;
  health?: HealthInfo;
  queues?: QueueInfo[];
  downloads?: DownloadInfo[];
}

interface DataStoreState {
  queues: QueueInfo[];
  health: HealthInfo | null;
  downloads: DownloadInfo[];
  status: DataStatus;
  error: string | null;
  selectedQueueId: string | null;
  selectedDownloadId: string | null;
  setSelectedQueueId: (id: string | null) => void;
  setSelectedDownloadId: (id: string | null) => void;
  applySnapshot: (snapshot: SnapshotPayload) => void;
  refresh: () => Promise<void>;
  clear: () => void;
}

export const useDataStore = create<DataStoreState>()((set, get) => ({
  queues: [],
  health: null,
  downloads: [],
  status: "idle",
  error: null,
  selectedQueueId: null,
  selectedDownloadId: null,

  setSelectedQueueId: (id) => {
    appLogger.gui.debug("Queue selected", id ?? "default");
    set({ selectedQueueId: id, selectedDownloadId: null });
  },

  setSelectedDownloadId: (id) => set({ selectedDownloadId: id }),

  applySnapshot: (snapshot) => {
    set((state) => ({
      queues: snapshot.queues ?? state.queues,
      health: snapshot.health ?? state.health,
      downloads: snapshot.downloads ?? state.downloads,
      status: "idle",
      error: null,
    }));
  },

  refresh: async () => {
    const client = useConnectionStore.getState().client;
    const connection = useConnectionStore.getState().connection;
    if (!client || connection !== "connected") {
      return;
    }

    const hasData =
      get().queues.length > 0 ||
      get().downloads.length > 0 ||
      get().health !== null;
    if (!hasData) {
      set({ status: "loading", error: null });
    }

    try {
      appLogger.gui.debug("Refreshing data from daemon");
      const [queueResult, health, downloadResult] = await Promise.all([
        client.listQueues(),
        client.health(),
        client.listDownloads(),
      ]);
      appLogger.gui.debug(
        `Data refresh OK (${queueResult.length} queues, ${downloadResult.length} downloads)`,
      );
      set({
        queues: queueResult,
        health,
        downloads: downloadResult,
        status: "idle",
        error: null,
      });
    } catch (err) {
      const message = err instanceof Error ? err.message : "Unknown error";
      appLogger.gui.error("Data refresh failed", message);
      set({
        status: "error",
        error: message,
      });
    }
  },

  clear: () =>
    set({
      queues: [],
      health: null,
      downloads: [],
      status: "idle",
      error: null,
      selectedDownloadId: null,
    }),
}));

export function selectSelectedDownload(
  downloads: DownloadInfo[],
  selectedId: string | null,
): DownloadInfo | null {
  if (!selectedId) {
    return null;
  }
  return downloads.find((d) => d.id === selectedId) ?? null;
}

export function selectEffectiveQueueId(state: DataStoreState): string {
  return state.selectedQueueId ?? DEFAULT_QUEUE_ID;
}

export function selectDownloadsForQueue(
  downloads: DownloadInfo[],
  queueId: string | null,
): DownloadInfo[] {
  if (isDefaultQueue(queueId)) {
    return downloads.filter((item) => !item.queueId);
  }
  if (!queueId) {
    return downloads;
  }
  return downloads.filter((item) => item.queueId === queueId);
}

export function selectDownloadCounts(
  queues: QueueInfo[],
  downloads: DownloadInfo[],
): Record<string, number> {
  const counts: Record<string, number> = {};
  for (const queue of queues) {
    counts[queue.id] = 0;
  }
  for (const item of downloads) {
    const key = item.queueId ?? DEFAULT_QUEUE_ID;
    if (counts[key] !== undefined) {
      counts[key] += 1;
    }
  }
  return counts;
}

export function selectSelectedQueue(
  queues: QueueInfo[],
  queueId: string | null,
): QueueInfo | null {
  if (!queueId) {
    return null;
  }
  return queues.find((q) => q.id === queueId) ?? null;
}
