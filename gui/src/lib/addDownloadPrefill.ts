import { getExtensionBridgeUrl } from "@/lib/browserExtensionBridge";

const ADD_DOWNLOAD_STORAGE_PREFIX = "avar.popup.add-download.";

export interface AddDownloadPrefill {
  url: string;
  filename?: string;
  referer?: string;
  streamKind?: string;
  defaultQueueId?: string | null;
  pageTitle?: string;
}

export function normalizeAddDownloadPrefill(payload: AddDownloadPrefill): AddDownloadPrefill {
  return {
    url: payload.url?.trim() ?? "",
    filename: payload.filename?.trim() || undefined,
    referer: payload.referer?.trim() || undefined,
    streamKind: payload.streamKind?.trim() || undefined,
    defaultQueueId: payload.defaultQueueId ?? null,
    pageTitle: payload.pageTitle?.trim() || undefined,
  };
}

export function stashAddDownloadPrefill(id: string, payload: AddDownloadPrefill): void {
  const normalized = normalizeAddDownloadPrefill(payload);
  const key = `${ADD_DOWNLOAD_STORAGE_PREFIX}${id}`;
  const raw = JSON.stringify(normalized);
  sessionStorage.setItem(key, raw);
  localStorage.setItem(key, raw);
}

export function readStashedAddDownloadPrefill(id: string): AddDownloadPrefill | null {
  const raw =
    sessionStorage.getItem(`${ADD_DOWNLOAD_STORAGE_PREFIX}${id}`) ??
    localStorage.getItem(`${ADD_DOWNLOAD_STORAGE_PREFIX}${id}`);
  if (!raw) {
    return null;
  }
  try {
    return normalizeAddDownloadPrefill(JSON.parse(raw) as AddDownloadPrefill);
  } catch {
    return null;
  }
}

export async function fetchAddDownloadPrefillFromBridge(
  addId: string,
): Promise<AddDownloadPrefill | null> {
  try {
    const bridgeUrl = await getExtensionBridgeUrl();
    const response = await fetch(
      `${bridgeUrl}/extension/add-download/${encodeURIComponent(addId)}`,
    );
    if (!response.ok) {
      return null;
    }
    const body = (await response.json()) as { ok?: boolean; payload?: AddDownloadPrefill };
    if (!body.ok || !body.payload) {
      return null;
    }
    return normalizeAddDownloadPrefill(body.payload);
  } catch {
    return null;
  }
}

export async function loadAddDownloadPrefill(addId: string): Promise<AddDownloadPrefill | null> {
  const stashed = readStashedAddDownloadPrefill(addId);
  if (stashed) {
    return stashed;
  }
  return fetchAddDownloadPrefillFromBridge(addId);
}
