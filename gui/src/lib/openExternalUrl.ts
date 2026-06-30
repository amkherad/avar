const HTTP_URL_PATTERN = /^https?:\/\//i;

function normalizeHttpUrl(url: string): string | null {
  const trimmed = url.trim();
  return HTTP_URL_PATTERN.test(trimmed) ? trimmed : null;
}

export async function openExternalUrl(url: string): Promise<boolean> {
  const normalized = normalizeHttpUrl(url);
  if (!normalized) {
    return false;
  }

  if (window.avar?.openExternal) {
    return window.avar.openExternal(normalized);
  }

  window.open(normalized, "_blank", "noopener,noreferrer");
  return true;
}
