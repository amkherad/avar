import i18n from "@/i18n";
import { formatDownloadStatus } from "@/lib/downloadStatusLabel";
import { getServiceWorkerRegistration } from "@/lib/pwa";
import { useConfigStore } from "@/stores/configStore";
import type { DownloadInfo, QueueInfo } from "@/api/types";

export type NotificationCategory = "download" | "queue" | "general";

export interface AppNotification {
  title: string;
  body?: string;
  category?: NotificationCategory;
  tag?: string;
}

const NOTIFY_DOWNLOAD_STATUSES = new Set(["completed", "error", "paused", "cancelled"]);

const lastNotifiedDownloadStatus = new Map<string, string>();

let permissionRequested = false;
let connectionNotifyTimer: ReturnType<typeof setTimeout> | null = null;
let pendingConnectionNotify: (() => void) | null = null;

function isElectron(): boolean {
  return window.avar?.isElectron === true;
}

export async function requestNotificationPermission(): Promise<NotificationPermission | "unsupported"> {
  if (isElectron()) {
    return "granted";
  }
  if (!("Notification" in window)) {
    return "unsupported";
  }
  if (Notification.permission === "granted" || Notification.permission === "denied") {
    return Notification.permission;
  }
  if (permissionRequested) {
    return Notification.permission;
  }
  permissionRequested = true;
  return Notification.requestPermission();
}

async function showViaServiceWorker(notification: AppNotification): Promise<boolean> {
  const registration = await getServiceWorkerRegistration();
  if (!registration?.showNotification) {
    return false;
  }

  const tag = notification.tag ?? `avar-${notification.category ?? "general"}`;
  await registration.showNotification(notification.title, {
    body: notification.body,
    icon: "./icon.svg",
    badge: "./icon.svg",
    tag,
  });
  return true;
}

export async function showNotification(notification: AppNotification): Promise<void> {
  if (!useConfigStore.getState().config.notificationsEnabled) {
    return;
  }

  const { title, body } = notification;
  const tag = notification.tag ?? `avar-${notification.category ?? "general"}`;

  if (isElectron() && window.avar?.showNotification) {
    await window.avar.showNotification({ title, body, tag });
    return;
  }

  if (!("Notification" in window)) {
    return;
  }

  if (Notification.permission === "default") {
    await requestNotificationPermission();
  }

  if (Notification.permission !== "granted") {
    return;
  }

  const shown = await showViaServiceWorker(notification);
  if (shown) {
    return;
  }

  new Notification(title, { body, tag });
}

export function clearNotifiedDownloadStatuses(removedIds?: Iterable<string>): void {
  if (removedIds) {
    for (const id of removedIds) {
      lastNotifiedDownloadStatus.delete(id);
    }
    return;
  }
  lastNotifiedDownloadStatus.clear();
}

export function notifyDownloadStatusChange(
  download: DownloadInfo,
  previousStatus?: string,
): void {
  if (previousStatus === download.status) {
    return;
  }
  if (!NOTIFY_DOWNLOAD_STATUSES.has(download.status)) {
    return;
  }
  if (lastNotifiedDownloadStatus.get(download.id) === download.status) {
    return;
  }

  lastNotifiedDownloadStatus.set(download.id, download.status);

  const statusLabel = formatDownloadStatus(download.status, i18n.t.bind(i18n));
  void showNotification({
    category: "download",
    tag: `avar-download-${download.id}`,
    title: i18n.t("notifications.downloadStatus", {
      name: download.filename,
      status: statusLabel,
    }),
    body: download.url,
  });
}

export function notifyQueueStateChange(queue: QueueInfo, running: boolean): void {
  void showNotification({
    category: "queue",
    tag: `avar-queue-${queue.id}`,
    title: running
      ? i18n.t("notifications.queueStarted", { name: queue.name })
      : i18n.t("notifications.queueStopped", { name: queue.name }),
  });
}

function scheduleConnectionNotification(notify: () => void): void {
  pendingConnectionNotify = notify;
  if (connectionNotifyTimer) {
    return;
  }
  connectionNotifyTimer = setTimeout(() => {
    connectionNotifyTimer = null;
    const pending = pendingConnectionNotify;
    pendingConnectionNotify = null;
    pending?.();
  }, 3000);
}

export function notifyConnectionLost(): void {
  scheduleConnectionNotification(() => {
    void showNotification({
      category: "general",
      tag: "avar-connection",
      title: i18n.t("notifications.connectionLost"),
    });
  });
}

export function notifyConnectionRestored(): void {
  scheduleConnectionNotification(() => {
    void showNotification({
      category: "general",
      tag: "avar-connection",
      title: i18n.t("notifications.connectionRestored"),
    });
  });
}
