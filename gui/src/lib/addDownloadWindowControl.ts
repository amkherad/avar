import i18n from "@/i18n";
import {
  type AddDownloadWindowGuardReason,
  isAddDownloadPopupHash,
  releaseAddDownloadSlot,
  tryAcquireAddDownloadSlot,
} from "@/lib/addDownloadWindowGuard";
import { showNotification } from "@/lib/notificationService";
import { appLogger } from "@/lib/appLogger";

export interface OpenAddDownloadPopupOptions {
  fromExtensionGrab?: boolean;
}

function guardReasonMessage(reason: AddDownloadWindowGuardReason): string {
  switch (reason) {
    case "tooManyOpen":
      return i18n.t("download.tooManyAddWindows");
    case "grabRateLimited":
      return i18n.t("download.grabRateLimited");
  }
}

export function notifyAddDownloadWindowBlocked(reason: AddDownloadWindowGuardReason): void {
  const message = guardReasonMessage(reason);
  appLogger.gui.info("Add download window blocked", reason);
  void showNotification({
    title: i18n.t("download.add"),
    body: message,
    category: "general",
    tag: `avar-add-download-blocked-${reason}`,
  });
}

export function tryOpenAddDownloadPopup(
  options: OpenAddDownloadPopupOptions = {},
): boolean {
  const result = tryAcquireAddDownloadSlot({
    fromExtensionGrab: options.fromExtensionGrab,
  });
  if (!result.allowed && result.reason) {
    notifyAddDownloadWindowBlocked(result.reason);
    return false;
  }
  return true;
}

export function releaseAddDownloadPopupSlot(urlOrHash: string): void {
  if (isAddDownloadPopupHash(urlOrHash)) {
    releaseAddDownloadSlot();
  }
}

export function addDownloadWindowBlockedMessage(reason: AddDownloadWindowGuardReason): string {
  return guardReasonMessage(reason);
}
