/**
 * Main-process guard for add-download popup windows and extension grab bursts.
 * Keep in sync with src/lib/addDownloadWindowGuard.ts.
 */

const MAX_OPEN_ADD_DOWNLOAD_WINDOWS = 5;
const MAX_EXTENSION_GRABS_IN_WINDOW = 5;
const EXTENSION_GRAB_WINDOW_MS = 30_000;

/** @type {number} */
let openAddDownloadWindowCount = 0;

/** @type {number[]} */
const recentExtensionGrabTimestamps = [];

function pruneOldExtensionGrabs(now) {
  while (
    recentExtensionGrabTimestamps.length > 0 &&
    recentExtensionGrabTimestamps[0] < now - EXTENSION_GRAB_WINDOW_MS
  ) {
    recentExtensionGrabTimestamps.shift();
  }
}

function countOpenAddDownloadWindows() {
  return openAddDownloadWindowCount;
}

function countRecentExtensionGrabs(now = Date.now()) {
  pruneOldExtensionGrabs(now);
  return recentExtensionGrabTimestamps.length;
}

/**
 * @param {{ fromExtensionGrab?: boolean }} [options]
 * @returns {{ allowed: boolean, reason?: 'tooManyOpen' | 'grabRateLimited' }}
 */
function evaluateAddDownloadSlot(options = {}) {
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

/**
 * @param {{ fromExtensionGrab?: boolean }} [options]
 * @returns {{ allowed: boolean, reason?: 'tooManyOpen' | 'grabRateLimited' }}
 */
function tryAcquireAddDownloadSlot(options = {}) {
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

function releaseAddDownloadSlot() {
  openAddDownloadWindowCount = Math.max(0, openAddDownloadWindowCount - 1);
}

function isAddDownloadPopupHash(hash) {
  return typeof hash === "string" && hash.includes("/popup/add-download/");
}

function resetAddDownloadWindowGuardForTests() {
  openAddDownloadWindowCount = 0;
  recentExtensionGrabTimestamps.length = 0;
}

function addDownloadWindowBlockedMessage(reason) {
  switch (reason) {
    case "tooManyOpen":
      return "Too many add download windows are already open. Close one and try again.";
    case "grabRateLimited":
      return "Too many download grabs in a short time. Wait a moment and try again.";
    default:
      return "Could not open add download window.";
  }
}

module.exports = {
  MAX_OPEN_ADD_DOWNLOAD_WINDOWS,
  MAX_EXTENSION_GRABS_IN_WINDOW,
  EXTENSION_GRAB_WINDOW_MS,
  countOpenAddDownloadWindows,
  countRecentExtensionGrabs,
  evaluateAddDownloadSlot,
  tryAcquireAddDownloadSlot,
  releaseAddDownloadSlot,
  isAddDownloadPopupHash,
  addDownloadWindowBlockedMessage,
  resetAddDownloadWindowGuardForTests,
};
