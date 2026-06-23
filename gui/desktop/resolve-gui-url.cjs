/**
 * Resolve the GUI entry URL for any desktop shell (dev server, bundled, or dist).
 */

const path = require("node:path");
const {
  BUNDLED_GUI_URL,
  VITE_DEV_SERVER_URL,
  distDir,
  electronDir,
} = require("./env.cjs");

function resolveGuiIndexPath() {
  return path.join(distDir, "index.html");
}

function normalizeHash(hash = "") {
  if (!hash) {
    return "";
  }
  return hash.startsWith("#") ? hash.slice(1) : hash;
}

function extractHashFromUrl(url) {
  if (!url || typeof url !== "string") {
    return "";
  }
  const hashIndex = url.indexOf("#");
  return hashIndex >= 0 ? url.slice(hashIndex) : "";
}

/**
 * @param {{ hash?: string, isPackaged?: boolean }} [options]
 * @returns {string}
 */
function resolveGuiUrl(options = {}) {
  const hashPart = normalizeHash(options.hash ?? "");
  const hashSuffix = hashPart ? `#${hashPart}` : "";

  if (BUNDLED_GUI_URL) {
    return hashSuffix ? `${BUNDLED_GUI_URL}${hashSuffix}` : BUNDLED_GUI_URL;
  }

  if (!options.isPackaged && VITE_DEV_SERVER_URL) {
    return hashSuffix
      ? `${VITE_DEV_SERVER_URL}${hashSuffix}`
      : VITE_DEV_SERVER_URL;
  }

  if (hashPart) {
    const indexPath = resolveGuiIndexPath();
    return `file://${indexPath}${hashSuffix}`;
  }

  return `file://${resolveGuiIndexPath()}`;
}

/**
 * Electron BrowserWindow helpers — same resolution rules as resolveGuiUrl.
 * @param {{ hash?: string, isPackaged?: boolean, isDev?: boolean }} [options]
 */
function resolveElectronLoadTarget(options = {}) {
  const hashPart = normalizeHash(options.hash ?? "");

  if (BUNDLED_GUI_URL) {
    const url = hashPart ? `${BUNDLED_GUI_URL}#${hashPart}` : BUNDLED_GUI_URL;
    return { type: "url", url };
  }

  if (options.isDev && VITE_DEV_SERVER_URL) {
    const url = hashPart
      ? `${VITE_DEV_SERVER_URL}#${hashPart}`
      : VITE_DEV_SERVER_URL;
    return { type: "url", url };
  }

  if (hashPart) {
    return { type: "file", path: resolveGuiIndexPath(), hash: hashPart };
  }

  return { type: "file", path: resolveGuiIndexPath() };
}

function loadElectronWindowContent(win, options = {}) {
  const target = resolveElectronLoadTarget(options);
  if (target.type === "url") {
    void win.loadURL(target.url);
    return;
  }
  if (target.hash) {
    void win.loadFile(target.path, { hash: target.hash });
    return;
  }
  void win.loadFile(target.path);
}

module.exports = {
  electronDir,
  resolveGuiIndexPath,
  normalizeHash,
  extractHashFromUrl,
  resolveGuiUrl,
  resolveElectronLoadTarget,
  loadElectronWindowContent,
};
