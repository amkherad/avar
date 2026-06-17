import i18n from "@/i18n";
import { formatDownloadStatus } from "@/lib/downloadStatusLabel";
import type { DownloadInfo, QueueInfo } from "@/api/types";

export type NotificationCategory = "download" | "queue" | "general";

export interface AppNotification {
  title: string;
  body?: string;
  category?: NotificationCategory;
}

let permissionRequested = false;

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

export async function showNotification(notification: AppNotification): Promise<void> {
  const { title, body } = notification;

  if (isElectron() && window.avar?.showNotification) {
    await window.avar.showNotification({ title, body });
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

  new Notification(title, { body, tag: `avar-${notification.category ?? "general"}` });
}

export function notifyDownloadStatusChange(
  download: DownloadInfo,
  previousStatus?: string,
): void {
  if (previousStatus === download.status) {
    return;
  }

  const statusLabel = formatDownloadStatus(download.status, i18n.t.bind(i18n));
  void showNotification({
    category: "download",
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
    title: running
      ? i18n.t("notifications.queueStarted", { name: queue.name })
      : i18n.t("notifications.queueStopped", { name: queue.name }),
  });
}

export function notifyConnectionLost(): void {
  void showNotification({
    category: "general",
    title: i18n.t("notifications.connectionLost"),
  });
}

export function notifyConnectionRestored(): void {
  void showNotification({
    category: "general",
    title: i18n.t("notifications.connectionRestored"),
  });
}
