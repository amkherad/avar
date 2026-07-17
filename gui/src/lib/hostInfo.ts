export type HostShellId = "electron" | "tiny" | "electrobun" | "browser";

export interface HostInfo {
  shell: HostShellId;
  platform?: string;
  versions: Record<string, string>;
}

declare global {
  interface Window {
    __AVAR_HOST__?: HostInfo;
  }
}

const VERSION_LABEL_KEYS: Record<string, string> = {
  electron: "settings.about.versionElectron",
  chromium: "settings.about.versionChromium",
  node: "settings.about.versionNode",
  tinytron: "settings.about.versionTinytron",
  electrobun: "settings.about.versionElectrobun",
  bun: "settings.about.versionBun",
};

export function getHostVersionLabelKey(versionKey: string): string {
  return VERSION_LABEL_KEYS[versionKey] ?? "settings.about.versionUnknown";
}

export function getHostInfo(): HostInfo {
  if (window.avar?.getHostInfo) {
    const info = window.avar.getHostInfo();
    if (info && typeof info === "object" && "shell" in info) {
      return info;
    }
  }

  if (window.__AVAR_HOST__) {
    return window.__AVAR_HOST__;
  }

  return {
    shell: "browser",
    versions: {},
  };
}

export async function resolveHostInfo(): Promise<HostInfo> {
  if (window.avar?.getHostInfo) {
    return Promise.resolve(window.avar.getHostInfo());
  }

  if (window.__AVAR_HOST__) {
    return window.__AVAR_HOST__;
  }

  return {
    shell: "browser",
    versions: {},
  };
}

export function getHostShellLabelKey(shell: HostShellId): string {
  return `settings.about.hostShell.${shell}`;
}

const PLATFORM_LABEL_KEYS: Record<string, string> = {
  win32: "settings.about.platformWindows",
  darwin: "settings.about.platformMac",
  linux: "settings.about.platformLinux",
};

export function getHostPlatformLabelKey(platform?: string): string | null {
  if (!platform) {
    return null;
  }
  return PLATFORM_LABEL_KEYS[platform] ?? null;
}

export function listHostVersionEntries(
  host: HostInfo,
): { key: string; value: string }[] {
  return Object.entries(host.versions).map(([key, value]) => ({ key, value }));
}
