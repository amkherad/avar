export function refererHost(url?: string | null): string | null {  if (!url?.trim()) {
    return null;
  }
  try {
    return new URL(url.trim()).hostname.toLowerCase();
  } catch {
    return null;
  }
}

export function referrersLikelyMatch(
  existingReferer?: string | null,
  newReferer?: string | null,
): boolean {  const existingHost = refererHost(existingReferer);
  const newHost = refererHost(newReferer);
  if (!existingHost || !newHost) {
    return true;
  }
  return existingHost === newHost;
}

export function downloadRefererUrl(
  details?: { referer?: string; originalPage?: string },
  download?: { referer?: string },
): string | undefined {
  const value =
    details?.referer?.trim() ||
    details?.originalPage?.trim() ||
    download?.referer?.trim() ||
    "";
  return value || undefined;
}

export function fileSizesMismatch(persistedBytes: number, probedBytes: number): boolean {  if (persistedBytes <= 0 || probedBytes <= 0) {
    return false;
  }
  return persistedBytes !== probedBytes;
}
