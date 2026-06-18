export type BrowserId = "chrome" | "firefox" | "edge" | "opera";

export type ExtensionVariant = "chromium" | "firefox";

export function getExtensionVariant(browser: BrowserId): ExtensionVariant {
  return browser === "firefox" ? "firefox" : "chromium";
}

export function getExtensionZipUrl(browser: BrowserId): string {
  const variant = getExtensionVariant(browser);
  const base = import.meta.env.BASE_URL.replace(/\/?$/, "/");
  return `${base}extensions/packages/avar-${variant}.zip`;
}

export function getExtensionsManagementUrl(browser: BrowserId): string {
  switch (browser) {
    case "firefox":
      return "about:addons";
    case "edge":
      return "edge://extensions";
    case "opera":
      return "opera://extensions";
    default:
      return "chrome://extensions";
  }
}

export function detectCurrentBrowser(): BrowserId | null {
  const ua = navigator.userAgent;
  if (ua.includes("Firefox/")) {
    return "firefox";
  }
  if (ua.includes("Edg/")) {
    return "edge";
  }
  if (ua.includes("OPR/") || ua.includes("Opera")) {
    return "opera";
  }
  if (ua.includes("Chrome/") || ua.includes("Chromium/")) {
    return "chrome";
  }
  return null;
}

export async function downloadExtensionPackage(browser: BrowserId): Promise<void> {
  const url = getExtensionZipUrl(browser);
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to fetch extension package (${response.status})`);
  }

  const blob = await response.blob();
  const objectUrl = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = objectUrl;
  anchor.download = `avar-${getExtensionVariant(browser)}.zip`;
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  URL.revokeObjectURL(objectUrl);
}

export function tryOpenExtensionsPage(browser: BrowserId): void {
  const url = getExtensionsManagementUrl(browser);
  const opened = window.open(url, "_blank");
  if (!opened) {
    void navigator.clipboard?.writeText(url);
  }
}

export async function installBrowserExtension(browser: BrowserId): Promise<void> {
  await downloadExtensionPackage(browser);
  tryOpenExtensionsPage(browser);
}
