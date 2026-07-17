/** True when running inside the Electron desktop shell. */
export function isElectronShell(): boolean {
  if (window.avar?.isElectron === true) {
    return true;
  }

  return typeof navigator !== "undefined" && /\bElectron\//i.test(navigator.userAgent);
}

/** macOS Electron window — traffic-light controls belong on the left. */
export function isElectronMacDesktop(): boolean {
  if (!isElectronShell()) {
    return false;
  }

  if (window.avar?.platform === "darwin") {
    return true;
  }

  return /Mac/i.test(navigator.platform);
}

/** True when running in any native desktop shell (Electron, Tiny, Electrobun). */
export function isDesktopShell(): boolean {
  return isElectronShell() || window.__AVAR_HOST__?.shell === "tiny" || window.__AVAR_HOST__?.shell === "electrobun";
}
