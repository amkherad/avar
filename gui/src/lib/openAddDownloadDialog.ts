import { openAddDownloadPopup } from "@/lib/popup";

export function openAddDownloadDialog(title: string, defaultQueueId?: string | null): void {
  void openAddDownloadPopup(
    {
      url: "",
      defaultQueueId: defaultQueueId ?? null,
    },
    title,
  );
}
