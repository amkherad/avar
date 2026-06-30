import { openAddDownloadPopup } from "@/lib/popup";
import { appLogger } from "@/lib/appLogger";

export function openAddDownloadDialog(title: string, defaultQueueId?: string | null): void {
  appLogger.gui.debug("Add download dialog opened", defaultQueueId ?? "default");
  void openAddDownloadPopup(
    {
      url: "",
      defaultQueueId: defaultQueueId ?? null,
    },
    title,
  );
}
