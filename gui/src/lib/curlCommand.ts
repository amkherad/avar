import type { DownloadInfo } from "@/api/types";

export function buildDownloadCurl(download: DownloadInfo): string {
  if (!download.url) {
    return "";
  }

  const parts = ["curl", "-L", "-o", JSON.stringify(download.filename || "download")];
  parts.push(JSON.stringify(download.url));
  return parts.join(" ");
}

export async function copyTextToClipboard(text: string): Promise<boolean> {
  if (!text) {
    return false;
  }

  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch {
    return false;
  }
}
