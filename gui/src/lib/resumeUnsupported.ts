import type { DownloadInfo } from "@/api/types";

export const RESUME_UNSUPPORTED_DESCRIPTION = "resume_unsupported";

export function isResumeUnsupportedDownload(download: DownloadInfo): boolean {
  return (
    download.description === RESUME_UNSUPPORTED_DESCRIPTION &&
    (download.status === "stopped" || download.status === "error")
  );
}
