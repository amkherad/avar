export const MAX_OPEN_ADD_DOWNLOAD_WINDOWS = 5;
export const MAX_EXTENSION_GRABS_IN_WINDOW = 5;
/** Sliding window for extension grab bursts. */
export const EXTENSION_GRAB_WINDOW_MS = 30_000;

export type AddDownloadWindowGuardReason = "tooManyOpen" | "grabRateLimited";

export interface AddDownloadWindowGuardResult {
  allowed: boolean;
  reason?: AddDownloadWindowGuardReason;
}

export interface TryAcquireAddDownloadSlotOptions {
  /** Counts toward the grab burst limit (browser extension captures). */
  fromExtensionGrab?: boolean;
}

let openAddDownloadWindowCount = 0;
const recentExtensionGrabTimestamps: number[] = [];

function pruneOldExtensionGrabs(now: number): void {
  while (
    recentExtensionGrabTimestamps.length > 0 &&
    recentExtensionGrabTimestamps[0] < now - EXTENSION_GRAB_WINDOW_MS
  ) {
    recentExtensionGrabTimestamps.shift();
  }
}

export function countOpenAddDownloadWindows(): number {
  return openAddDownloadWindowCount;
}

export function countRecentExtensionGrabs(now = Date.now()): number {
  pruneOldExtensionGrabs(now);
  return recentExtensionGrabTimestamps.length;
}

export function evaluateAddDownloadSlot(
  options: TryAcquireAddDownloadSlotOptions = {},
): AddDownloadWindowGuardResult {
  if (openAddDownloadWindowCount >= MAX_OPEN_ADD_DOWNLOAD_WINDOWS) {
    return { allowed: false, reason: "tooManyOpen" };
  }

  if (options.fromExtensionGrab) {
    const now = Date.now();
    pruneOldExtensionGrabs(now);
    if (recentExtensionGrabTimestamps.length >= MAX_EXTENSION_GRABS_IN_WINDOW) {
      return { allowed: false, reason: "grabRateLimited" };
    }
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

  if (options.fromExtensionGrab) {
    recentExtensionGrabTimestamps.push(Date.now());
  }

  openAddDownloadWindowCount += 1;
  return { allowed: true };
}

export function releaseAddDownloadSlot(): void {
  openAddDownloadWindowCount = Math.max(0, openAddDownloadWindowCount - 1);
}

export function isAddDownloadPopupHash(hash: string): boolean {
  return hash.includes("/popup/add-download/");
}

/** Reset guard state (tests only). */
export function resetAddDownloadWindowGuardForTests(): void {
  openAddDownloadWindowCount = 0;
  recentExtensionGrabTimestamps.length = 0;
}
