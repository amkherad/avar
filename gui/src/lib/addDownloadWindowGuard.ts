export const MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW = 5;
/** Sliding window for add-download popup bursts (manual + extension). */
export const ADD_DOWNLOAD_OPEN_WINDOW_MS = 30_000;

export type AddDownloadWindowGuardReason = "rateLimited";

export interface AddDownloadWindowGuardResult {
  allowed: boolean;
  reason?: AddDownloadWindowGuardReason;
}

export interface TryAcquireAddDownloadSlotOptions {
  /** @deprecated All opens share the same rate limit; kept for caller compatibility. */
  fromExtensionGrab?: boolean;
}

const recentAddDownloadOpenTimestamps: number[] = [];

function pruneOldAddDownloadOpens(now: number): void {
  while (
    recentAddDownloadOpenTimestamps.length > 0 &&
    recentAddDownloadOpenTimestamps[0] < now - ADD_DOWNLOAD_OPEN_WINDOW_MS
  ) {
    recentAddDownloadOpenTimestamps.shift();
  }
}

export function countRecentAddDownloadOpens(now = Date.now()): number {
  pruneOldAddDownloadOpens(now);
  return recentAddDownloadOpenTimestamps.length;
}

export function evaluateAddDownloadSlot(
  _options: TryAcquireAddDownloadSlotOptions = {},
): AddDownloadWindowGuardResult {
  const now = Date.now();
  pruneOldAddDownloadOpens(now);
  if (recentAddDownloadOpenTimestamps.length >= MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW) {
    return { allowed: false, reason: "rateLimited" };
  }

  return { allowed: true };
}

export function tryAcquireAddDownloadSlot(
  options: TryAcquireAddDownloadSlotOptions = {},
): AddDownloadWindowGuardResult {
  const evaluation = evaluateAddDownloadSlot(options);
  if (!evaluation.allowed) {
    return evaluation;
  }

  recentAddDownloadOpenTimestamps.push(Date.now());
  return { allowed: true };
}

/** No-op: rate limiting is based on open timestamps, not concurrent window count. */
export function releaseAddDownloadSlot(): void {}

export function isAddDownloadPopupHash(hash: string): boolean {
  return hash.includes("/popup/add-download/");
}

/** Reset guard state (tests only). */
export function resetAddDownloadWindowGuardForTests(): void {
  recentAddDownloadOpenTimestamps.length = 0;
}
