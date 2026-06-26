import type { DaemonClient } from "@/api/daemon";
import type { DownloadInfo } from "@/api/types";
import { isCompleted } from "@/lib/downloadStatus";

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

  const result = await client.resolveDownloadPath(download.id);
  if (result.exitCode !== 0 || !result.destPath) {
    throw new Error("Could not resolve download file path");
  }

  const openError = await window.avar.openPath(result.destPath);
  if (openError) {
    throw new Error(openError);
  }
}
