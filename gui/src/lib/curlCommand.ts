export function buildDownloadCurl(
  filename: string,
  url: string,
  referer?: string,
): string {
  if (!url) {
    return "";
  }

  const parts = ["curl", "-L", "-o", JSON.stringify(filename || "download")];
  if (referer) {
    parts.push("-e", JSON.stringify(referer));
  }
  parts.push(JSON.stringify(url));
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
