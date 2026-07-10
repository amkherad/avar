/**
 * Main-process guard for add-download popup windows and extension grab bursts.
 * Keep in sync with src/lib/addDownloadWindowGuard.ts.
 */

const MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW = 5;
const ADD_DOWNLOAD_OPEN_WINDOW_MS = 30_000;

/** @type {number[]} */
const recentAddDownloadOpenTimestamps = [];

function pruneOldAddDownloadOpens(now) {
  while (
    recentAddDownloadOpenTimestamps.length > 0 &&
    recentAddDownloadOpenTimestamps[0] < now - ADD_DOWNLOAD_OPEN_WINDOW_MS
  ) {
    recentAddDownloadOpenTimestamps.shift();
  }
}

function countRecentAddDownloadOpens(now = Date.now()) {
  pruneOldAddDownloadOpens(now);
  return recentAddDownloadOpenTimestamps.length;
}

/**
 * @param {{ fromExtensionGrab?: boolean }} [options]
 * @returns {{ allowed: boolean, reason?: 'rateLimited' }}
 */
function evaluateAddDownloadSlot(_options = {}) {
  const now = Date.now();
  pruneOldAddDownloadOpens(now);
  if (recentAddDownloadOpenTimestamps.length >= MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW) {
    return { allowed: false, reason: "rateLimited" };
  }

  return { allowed: true };
}

/**
 * @param {{ fromExtensionGrab?: boolean }} [options]
 * @returns {{ allowed: boolean, reason?: 'rateLimited' }}
 */
function tryAcquireAddDownloadSlot(options = {}) {
  const evaluation = evaluateAddDownloadSlot(options);
  if (!evaluation.allowed) {
    return evaluation;
  }

  recentAddDownloadOpenTimestamps.push(Date.now());
  return { allowed: true };
}

function releaseAddDownloadSlot() {}

function isAddDownloadPopupHash(hash) {
  return typeof hash === "string" && hash.includes("/popup/add-download/");
}

function resetAddDownloadWindowGuardForTests() {
  recentAddDownloadOpenTimestamps.length = 0;
}

function addDownloadWindowBlockedMessage(reason) {
  switch (reason) {
    case "rateLimited":
      return "Too many add download windows opened in a short time. Wait a moment and try again.";
    default:
      return "Could not open add download window.";
  }
}

module.exports = {
  MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW,
  ADD_DOWNLOAD_OPEN_WINDOW_MS,
  countRecentAddDownloadOpens,
  evaluateAddDownloadSlot,
  tryAcquireAddDownloadSlot,
  releaseAddDownloadSlot,
  isAddDownloadPopupHash,
  addDownloadWindowBlockedMessage,
  resetAddDownloadWindowGuardForTests,
};
