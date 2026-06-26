import type { DaemonClient } from "@/api/daemon";
import type { DownloadInfo } from "@/api/types";
import { isCompleted } from "@/lib/downloadStatus";
import { resolveDownloadDestPath } from "@/lib/resolveDownloadDestPath";

export async function openDownloadContainingFolder(
  client: DaemonClient,
  download: DownloadInfo,
): Promise<void> {
  if (!isCompleted(download.status)) {
    throw new Error("Download is not completed");
  }
  if (!window.avar?.showItemInFolder) {
    throw new Error("Opening folders is not supported in this environment");
  }

  const path = await resolveDownloadDestPath(client, download);

  let openError: string;
  try {
    openError = await window.avar.showItemInFolder(path);
  } catch (error: unknown) {
    const message = error instanceof Error ? error.message : String(error);
    if (message.includes("No handler registered")) {
      throw new Error(
        "Open containing folder is not available in this desktop shell. Restart the Electron app (npm run dev:desktop) or rebuild the desktop package.",
      );
    }
    throw error;
  }

  if (openError) {
    throw new Error(openError);
  }
}
