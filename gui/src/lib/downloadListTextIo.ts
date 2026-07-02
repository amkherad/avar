import type { DownloadInfo } from "@/api/types";

export function collectDownloadUrls(downloads: DownloadInfo[]): string[] {
  const seen = new Set<string>();
  const urls: string[] = [];

  for (const download of downloads) {
    const url = download.url?.trim();
    if (!url || seen.has(url)) {
      continue;
    }
    seen.add(url);
    urls.push(url);
  }

  return urls;
}

export function formatDownloadUrlsAsText(urls: string[]): string {
  if (urls.length === 0) {
    return "";
  }
  return `${urls.join("\n")}\n`;
}

export function exportDownloadUrlsToTextFile(
  urls: string[],
  filename = "downloads.txt",
): void {
  const text = formatDownloadUrlsAsText(urls);
  const blob = new Blob([text], { type: "text/plain;charset=utf-8" });
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
