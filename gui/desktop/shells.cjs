/**
 * Desktop shell registry — keeps Electron and experimental shells parallel.
 */

const SHELL_ELECTRON = "electron";
const SHELL_TINY = "tiny";

/** @type {Record<string, { label: string, capabilities: string[] }>} */
const SHELLS = {
  [SHELL_ELECTRON]: {
    label: "Electron",
    capabilities: [
      "preload-bridge",
      "daemon-proxy",
      "extension-bridge",
      "system-tray",
      "native-popups",
      "native-notifications",
      "directory-picker",
      "remote-file-save",
    ],
  },
  [SHELL_TINY]: {
    label: "Tiny (webview)",
    capabilities: ["daemon-proxy"],
  },
};

function shellHasCapability(shellId, capability) {
  return SHELLS[shellId]?.capabilities.includes(capability) ?? false;
}

module.exports = {
  SHELL_ELECTRON,
  SHELL_TINY,
  SHELLS,
  shellHasCapability,
};
