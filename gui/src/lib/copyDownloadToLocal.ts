import type { GuiSession } from "@/config/defaults";
import type { DownloadInfo } from "@/api/types";
import { buildDownloadFileUrl } from "@/api/daemon";
import { appLogger } from "@/lib/appLogger";

export interface CopyDownloadToLocalOptions {
  session: GuiSession;
  download: DownloadInfo;
  localDownloadPath: string;
}

function downloadHeaders(session: GuiSession): HeadersInit {
  const headers: Record<string, string> = {};
  if (session.authToken?.trim()) {
    headers.Authorization = `Bearer ${session.authToken.trim()}`;
  }
  return headers;
}

function triggerBrowserDownload(blob: Blob, filename: string): void {
  const objectUrl = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = objectUrl;
  anchor.download = filename;
  anchor.rel = "noopener";
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  window.setTimeout(() => URL.revokeObjectURL(objectUrl), 0);
}

export async function copyDownloadToLocal({
  session,
  download,
  localDownloadPath,
}: CopyDownloadToLocalOptions): Promise<void> {
  const fileUrl = buildDownloadFileUrl(session, download.id);
  const filename = download.filename || download.id;

  if (window.avar?.saveRemoteDownloadFile) {
    await window.avar.saveRemoteDownloadFile({
      fileUrl,
      authToken: session.authToken,
      destDir: localDownloadPath,
      filename,
    });
    appLogger.gui.info("Copied download to local folder", filename);
    return;
  }

  const response = await fetch(fileUrl, { headers: downloadHeaders(session) });
  if (!response.ok) {
    throw new Error(`Failed to fetch remote file (${response.status})`);
  }

  const blob = await response.blob();
  triggerBrowserDownload(blob, filename);
  appLogger.gui.info("Downloaded remote file via browser", filename);
}
