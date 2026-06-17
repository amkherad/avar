import type { DownloadInfo, QueueInfo } from "@/api/types";
import {
  notifyConnectionLost,
  notifyConnectionRestored,
  notifyDownloadStatusChange,
  notifyQueueStateChange,
  requestNotificationPermission,
} from "@/lib/notificationService";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";

function trackDownloadStatuses(downloads: DownloadInfo[]): Map<string, string> {
  const map = new Map<string, string>();
  for (const item of downloads) {
    map.set(item.id, item.status);
  }
  return map;
}

function trackQueueRunning(queues: QueueInfo[]): Map<string, boolean> {
  const map = new Map<string, boolean>();
  for (const queue of queues) {
    map.set(queue.id, queue.running);
  }
  return map;
}

export function initNotificationWatcher(): () => void {
  void requestNotificationPermission();

  let prevDownloads = trackDownloadStatuses(useDataStore.getState().downloads);
  let prevQueues = trackQueueRunning(useDataStore.getState().queues);
  let prevConnection = useConnectionStore.getState().connection;

  const unsubData = useDataStore.subscribe((state) => {
    for (const download of state.downloads) {
      const previous = prevDownloads.get(download.id);
      if (previous !== undefined && previous !== download.status) {
        notifyDownloadStatusChange(download, previous);
      }
    }
    prevDownloads = trackDownloadStatuses(state.downloads);

    for (const queue of state.queues) {
      const previous = prevQueues.get(queue.id);
      if (previous !== undefined && previous !== queue.running) {
        notifyQueueStateChange(queue, queue.running);
      }
    }
    prevQueues = trackQueueRunning(state.queues);
  });

  const unsubConnection = useConnectionStore.subscribe((state) => {
    if (state.connection === prevConnection) {
      return;
    }
    if (state.connection === "disconnected" && prevConnection === "connected") {
      notifyConnectionLost();
    }
    if (state.connection === "connected" && prevConnection !== "connected") {
      notifyConnectionRestored();
    }
    prevConnection = state.connection;
  });

  return () => {
    unsubData();
    unsubConnection();
  };
}
