import { create } from "zustand";
import type { DownloadInfo, HealthInfo, QueueInfo, SystemStatsInfo } from "@/api/types";
import { DEFAULT_QUEUE_ID, isDefaultQueue } from "@/queue/defaultQueue";
import { useConnectionStore } from "@/stores/connectionStore";
import { appLogger } from "@/lib/appLogger";

export type DataStatus = "idle" | "loading" | "error";

export interface SnapshotPayload {
  type?: string;
  health?: HealthInfo;
  queues?: QueueInfo[];
  downloads?: DownloadInfo[];
  stats?: SystemStatsInfo;
}

interface DataStoreState {
  queues: QueueInfo[];
  health: HealthInfo | null;
  downloads: DownloadInfo[];
  stats: SystemStatsInfo | null;
  status: DataStatus;
  error: string | null;
  selectedQueueId: string | null;
  selectedDownloadId: string | null;
  selectedDownloadIds: string[];
  selectionAnchorId: string | null;
  visibleDownloadOrder: string[];
  setSelectedQueueId: (id: string | null) => void;
  setSelectedDownloadId: (id: string | null) => void;
  setSelectedDownloadIds: (ids: string[]) => void;
  setVisibleDownloadOrder: (ids: string[]) => void;
  selectDownload: (id: string, options?: { additive?: boolean; range?: boolean; orderedIds?: string[] }) => void;
  clearDownloadSelection: () => void;
  applySnapshot: (snapshot: SnapshotPayload) => void;
  setStats: (stats: SystemStatsInfo | null) => void;
  refresh: () => Promise<void>;
  clear: () => void;
}

export const useDataStore = create<DataStoreState>()((set, get) => ({
  queues: [],
  health: null,
  downloads: [],
  stats: null,
  status: "idle",
  error: null,
  selectedQueueId: null,
  selectedDownloadId: null,
  selectedDownloadIds: [],
  selectionAnchorId: null,
  visibleDownloadOrder: [],

  setSelectedQueueId: (id) => {
    appLogger.gui.debug("Queue selected", id ?? "default");
    set({
      selectedQueueId: id,
      selectedDownloadId: null,
      selectedDownloadIds: [],
      selectionAnchorId: null,
    });
  },

  setSelectedDownloadId: (id) =>
    set(() => ({
      selectedDownloadId: id,
      selectedDownloadIds: id ? [id] : [],
      selectionAnchorId: id,
    })),

  setSelectedDownloadIds: (ids) =>
    set({
      selectedDownloadIds: ids,
      selectedDownloadId: ids[ids.length - 1] ?? null,
      selectionAnchorId: ids[ids.length - 1] ?? null,
    }),

  setVisibleDownloadOrder: (ids) => set({ visibleDownloadOrder: ids }),

  selectDownload: (id, options = {}) => {
    const { additive = false, range = false, orderedIds = [] } = options;
    const state = get();

    if (range && state.selectionAnchorId && orderedIds.length > 0) {
      const anchorIndex = orderedIds.indexOf(state.selectionAnchorId);
      const clickIndex = orderedIds.indexOf(id);
      if (anchorIndex >= 0 && clickIndex >= 0) {
        const start = Math.min(anchorIndex, clickIndex);
        const end = Math.max(anchorIndex, clickIndex);
        const rangeIds = orderedIds.slice(start, end + 1);
        set({
          selectedDownloadIds: rangeIds,
          selectedDownloadId: id,
        });
        return;
      }
    }

    if (additive) {
      const exists = state.selectedDownloadIds.includes(id);
      const nextIds = exists
        ? state.selectedDownloadIds.filter((item) => item !== id)
        : [...state.selectedDownloadIds, id];
      set({
        selectedDownloadIds: nextIds,
        selectedDownloadId: id,
        selectionAnchorId: id,
      });
      return;
    }

    set({
      selectedDownloadIds: [id],
      selectedDownloadId: id,
      selectionAnchorId: id,
    });
  },

  clearDownloadSelection: () =>
    set({
      selectedDownloadIds: [],
      selectedDownloadId: null,
      selectionAnchorId: null,
    }),

  applySnapshot: (snapshot) => {
    set((state) => ({
      queues: snapshot.queues ?? state.queues,
      health: snapshot.health ?? state.health,
      downloads: snapshot.downloads ?? state.downloads,
      stats: snapshot.stats ?? state.stats,
      status: "idle",
      error: null,
    }));
  },

  setStats: (stats) => set({ stats }),

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
      stats: null,
      status: "idle",
      error: null,
      selectedDownloadId: null,
      selectedDownloadIds: [],
      selectionAnchorId: null,
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

export function selectSelectedDownloads(
  downloads: DownloadInfo[],
  selectedIds: string[],
): DownloadInfo[] {
  const idSet = new Set(selectedIds);
  return downloads.filter((d) => idSet.has(d.id));
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
