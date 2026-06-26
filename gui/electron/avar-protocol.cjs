/**
 * Custom avar:// URL scheme — launch or focus the desktop app from the browser extension.
 */

const path = require("node:path");

const AVAR_PROTOCOL_SCHEME = "avar";
const AVAR_FOCUS_ACTIONS = new Set(["focus", "wake", "show"]);

/**
 * @param {string} rawUrl
 * @returns {{ action: string, params: Record<string, string> } | null}
 */
function parseAvarProtocolUrl(rawUrl) {
  try {
    const url = new URL(rawUrl);
    if (url.protocol !== `${AVAR_PROTOCOL_SCHEME}:`) {
      return null;
    }

    const action = (url.hostname || url.pathname.replace(/^\//, "") || "focus").toLowerCase();
    return {
      action,
      params: Object.fromEntries(url.searchParams.entries()),
    };
  } catch {
    return null;
  }
}

/**
 * @param {string[]} argv
 * @returns {string | undefined}
 */
function extractAvarProtocolUrl(argv) {
  return argv.find(
    (arg) => typeof arg === "string" && arg.toLowerCase().startsWith(`${AVAR_PROTOCOL_SCHEME}://`),
  );
}

/**
 * @param {import('electron').App} electronApp
 */
function registerAvarProtocol(electronApp) {
  if (process.defaultApp) {
    if (process.argv.length >= 2) {
      electronApp.setAsDefaultProtocolClient(
        AVAR_PROTOCOL_SCHEME,
        process.execPath,
        [path.resolve(process.argv[1])],
      );
      return;
    }
  }

  electronApp.setAsDefaultProtocolClient(AVAR_PROTOCOL_SCHEME);
}

/**
 * @param {string} action
 * @returns {boolean}
 */
function isFocusAction(action) {
  return AVAR_FOCUS_ACTIONS.has(action.toLowerCase());
}

module.exports = {
  AVAR_PROTOCOL_SCHEME,
  parseAvarProtocolUrl,
  extractAvarProtocolUrl,
  registerAvarProtocol,
  isFocusAction,
};
