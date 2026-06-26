import type { DaemonClient } from "@/api/daemon";
import type { DownloadInfo } from "@/api/types";
import { appLogger } from "@/lib/appLogger";

export async function resolveDownloadDestPath(
  client: DaemonClient,
  download: DownloadInfo,
): Promise<string> {
  let path = download.destPath?.trim() || "";
  if (path) {
    return path;
  }

  try {
    const result = await client.resolveDownloadPath(download.id);
    if (result.exitCode === 0 && result.destPath) {
      return result.destPath;
    }
  } catch (error: unknown) {
    const message = error instanceof Error ? error.message : String(error);
    if (!message.includes("Method not found")) {
      throw error;
    }
    appLogger.gui.debug("download.resolvePath unavailable; destPath not in sync payload", message);
  }

  throw new Error("Could not resolve download file path");
}
