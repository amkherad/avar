import type { DaemonClient } from "@/api/daemon";
import type { DownloadInfo } from "@/api/types";
import { appLogger } from "@/lib/appLogger";
import { isCompleted } from "@/lib/downloadStatus";
import { resolveDownloadDestPath } from "@/lib/resolveDownloadDestPath";

export async function openDownloadFile(
  client: DaemonClient,
  download: DownloadInfo,
): Promise<void> {
  if (!isCompleted(download.status)) {
    throw new Error("Download is not completed");
  }
  if (!window.avar?.openPath) {
    throw new Error("Opening files is not supported in this environment");
  }

  const path = await resolveDownloadDestPath(client, download);

  let openError: string;
  try {
    openError = await window.avar.openPath(path);
  } catch (error: unknown) {
    const message = error instanceof Error ? error.message : String(error);
    if (message.includes("No handler registered")) {
      throw new Error(
        "Open file is not available in this desktop shell. Restart the Electron app (npm run dev:desktop) or rebuild the desktop package.",
      );
    }
    throw error;
  }

  if (openError) {
    throw new Error(openError);
  }
  appLogger.gui.debug("Opened download file", download.filename);
}
