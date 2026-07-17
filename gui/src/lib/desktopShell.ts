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

/** True when running in the Electron desktop shell. */
export function isDesktopShell(): boolean {
  return isElectronShell();
}
