import type { DaemonClient } from "@/api/daemon";
import type { DownloadInfo } from "@/api/types";
import { isCompleted } from "@/lib/downloadStatus";
import { appLogger } from "@/lib/appLogger";

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

  let path = download.destPath?.trim() || "";
  if (!path) {
    try {
      const result = await client.resolveDownloadPath(download.id);
      if (result.exitCode === 0 && result.destPath) {
        path = result.destPath;
      }
    } catch (error: unknown) {
      const message = error instanceof Error ? error.message : String(error);
      if (!message.includes("Method not found")) {
        throw error;
      }
      appLogger.gui.debug("download.resolvePath unavailable; destPath not in sync payload", message);
    }
  }

  if (!path) {
    throw new Error("Could not resolve download file path");
  }

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
}
