import { getExtensionBridgeUrl } from "@/lib/browserExtensionBridge";

const BATCH_STORAGE_PREFIX = "avar.popup.batch.";

export interface BatchAddDownloadItem {
  id: string;
  url: string;
  filename?: string;
  linkName?: string;
  fileType?: string;
  originalUrl?: string;
  fileSize?: number | null;
  streamKind?: string;
  referer?: string;
}

export interface BatchAddDownloadsPayload {
  items: BatchAddDownloadItem[];
  defaultQueueId?: string | null;
  pageTitle?: string;
}

function normalizeBatchItem(
  item: BatchAddDownloadItem,
  index: number,
): BatchAddDownloadItem {
  const url = item.url?.trim() ?? "";
  const id = item.id?.trim() || url || `item-${index}`;
  return {
    ...item,
    id,
    url,
    filename: item.filename?.trim() || undefined,
    linkName: item.linkName?.trim() || undefined,
    fileType: item.fileType?.trim() || item.streamKind?.trim() || undefined,
    originalUrl: item.originalUrl?.trim() || item.referer?.trim() || undefined,
    referer: item.referer?.trim() || item.originalUrl?.trim() || undefined,
    fileSize: typeof item.fileSize === "number" ? item.fileSize : null,
  };
}

export function normalizeBatchAddPayload(
  payload: BatchAddDownloadsPayload,
): BatchAddDownloadsPayload {
  const seen = new Set<string>();
  const items: BatchAddDownloadItem[] = [];

  for (const [index, raw] of payload.items.entries()) {
    const item = normalizeBatchItem(raw, index);
    if (!item.url || seen.has(item.url)) {
      continue;
    }
    seen.add(item.url);
    items.push(item);
  }

  return {
    ...payload,
    items,
    defaultQueueId: payload.defaultQueueId ?? null,
  };
}

function guessFilenameFromUrl(url: string): string | undefined {
  try {
    const segment = new URL(url).pathname.split("/").filter(Boolean).pop();
    if (!segment) {
      return undefined;
    }
    return decodeURIComponent(segment.split("?")[0]);
  } catch {
    return undefined;
  }
}

function urlLineToBatchItem(url: string, index: number): BatchAddDownloadItem {
  const filename = guessFilenameFromUrl(url);
  return {
    id: url,
    url,
    filename,
    linkName: filename || url,
  };
}

export function parseUrlLinesToBatchItems(text: string): BatchAddDownloadItem[] {
  const lines = text
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean);

  const seen = new Set<string>();
  const items: BatchAddDownloadItem[] = [];

  for (const [index, line] of lines.entries()) {
    if (seen.has(line)) {
      continue;
    }
    seen.add(line);
    items.push(urlLineToBatchItem(line, index));
  }

  return normalizeBatchAddPayload({ items }).items;
}

export function stashBatchAddPayload(id: string, payload: BatchAddDownloadsPayload): void {
  const normalized = normalizeBatchAddPayload(payload);
  const key = `${BATCH_STORAGE_PREFIX}${id}`;
  const raw = JSON.stringify(normalized);
  sessionStorage.setItem(key, raw);
  localStorage.setItem(key, raw);
}

export function readStashedBatchAddPayload(id: string): BatchAddDownloadsPayload | null {
  const raw =
    sessionStorage.getItem(`${BATCH_STORAGE_PREFIX}${id}`) ??
    localStorage.getItem(`${BATCH_STORAGE_PREFIX}${id}`);
  if (!raw) {
    return null;
  }
  try {
    return normalizeBatchAddPayload(JSON.parse(raw) as BatchAddDownloadsPayload);
  } catch {
    return null;
  }
}

export async function fetchBatchAddPayloadFromBridge(
  batchId: string,
): Promise<BatchAddDownloadsPayload | null> {
  try {
    const bridgeUrl = await getExtensionBridgeUrl();
    const response = await fetch(`${bridgeUrl}/extension/batch/${encodeURIComponent(batchId)}`);
    if (!response.ok) {
      return null;
    }
    const body = (await response.json()) as { ok?: boolean; payload?: BatchAddDownloadsPayload };
    if (!body.ok || !body.payload) {
      return null;
    }
    return normalizeBatchAddPayload(body.payload);
  } catch {
    return null;
  }
}

export async function loadBatchAddPayload(batchId: string): Promise<BatchAddDownloadsPayload | null> {
  const stashed = readStashedBatchAddPayload(batchId);
  if (stashed) {
    return stashed;
  }
  return fetchBatchAddPayloadFromBridge(batchId);
}

export function formatBatchFileType(fileType: string | undefined, t: (key: string) => string): string {
  if (!fileType) {
    return "—";
  }
  const key = `download.batchAdd.fileTypes.${fileType}`;
  const translated = t(key);
  return translated === key ? fileType : translated;
}
