import { createRequire } from "node:module";
import { BrowserWindow } from "electrobun/bun";
import type { AvarPopupOptions } from "../../shared/avar-rpc";
import { APP_TITLE } from "./desktop-shell";
import { createAvarRpc } from "./avar-handlers";
import { appRoot, loadResolveGuiUrl } from "./vendor-paths";

const require = createRequire(import.meta.url);
const {
  tryAcquireAddDownloadSlot,
  releaseAddDownloadSlot,
  isAddDownloadPopupHash,
  addDownloadWindowBlockedMessage,
} = require(`${appRoot}/electron/add-download-window-guard.cjs`) as {
  tryAcquireAddDownloadSlot: (options?: {
    fromExtensionGrab?: boolean;
  }) => { allowed: boolean; reason?: "tooManyOpen" | "grabRateLimited" };
  releaseAddDownloadSlot: () => void;
  isAddDownloadPopupHash: (hash: string) => boolean;
  addDownloadWindowBlockedMessage: (reason?: "tooManyOpen" | "grabRateLimited") => string;
};

const PRELOAD_URL = "views://avarbridge/avar-preload.js";

let popupCounter = 0;
const popupWindows = new Map<number, BrowserWindow>();
let baseGuiUrl = "";
let isDevChannel = false;

export function configurePopupManager(options: {
  baseGuiUrl: string;
  isDevChannel: boolean;
}): void {
  baseGuiUrl = options.baseGuiUrl;
  isDevChannel = options.isDevChannel;
}

function createWindowRpc() {
  return createAvarRpc();
}

function normalizeHash(hash = ""): string {
  if (!hash) {
    return "";
  }
  return hash.startsWith("#") ? hash : `#${hash}`;
}

function resolvePopupUrl(options: AvarPopupOptions): string {
  const { extractHashFromUrl } = loadResolveGuiUrl();
  const hash = options.hash ?? extractHashFromUrl(options.url ?? "");
  const hashSuffix = normalizeHash(hash);

  if (baseGuiUrl.startsWith("http://") || baseGuiUrl.startsWith("https://")) {
    const withoutHash = baseGuiUrl.split("#")[0];
    return `${withoutHash}${hashSuffix}`;
  }

  if (hashSuffix) {
    return `views://mainview/index.html${hashSuffix}`;
  }

  return "views://mainview/index.html";
}

function shouldPopupStayOnTop(hash = ""): boolean {
  return (
    hash.includes("/popup/add-download/") || hash.includes("/popup/batch-add/")
  );
}

function focusPopupWindow(popup: BrowserWindow): void {
  popup.show();
  popup.activate();
}

function showAddDownloadWindowBlockedNotification(
  reason?: "tooManyOpen" | "grabRateLimited",
): void {
  console.warn(addDownloadWindowBlockedMessage(reason));
}

export function openPopup(options: AvarPopupOptions = {}): number | null {
  const width = options.width ?? 520;
  const height = options.height ?? 640;
  const { extractHashFromUrl } = loadResolveGuiUrl();
  const hash = options.hash ?? extractHashFromUrl(options.url ?? "");
  const alwaysOnTop = options.alwaysOnTop ?? shouldPopupStayOnTop(hash);
  const url = resolvePopupUrl(options);
  const isAddDownloadPopup = isAddDownloadPopupHash(hash);

  if (isAddDownloadPopup) {
    const guardResult = tryAcquireAddDownloadSlot({
      fromExtensionGrab: Boolean(options.fromExtensionGrab),
    });
    if (!guardResult.allowed) {
      showAddDownloadWindowBlockedNotification(guardResult.reason);
      return null;
    }
  }

  const popupId = ++popupCounter;

  const popup = new BrowserWindow({
    title: options.title ?? APP_TITLE,
    url,
    preload: PRELOAD_URL,
    rpc: createWindowRpc(),
    frame: {
      width,
      height,
      x: 120,
      y: 120,
    },
    activate: true,
  });

  if (alwaysOnTop && typeof popup.setAlwaysOnTop === "function") {
    popup.setAlwaysOnTop(true);
  }

  popupWindows.set(popupId, popup);

  popup.on("close", () => {
    popupWindows.delete(popupId);
    if (isAddDownloadPopup) {
      releaseAddDownloadSlot();
    }
  });

  focusPopupWindow(popup);

  if (isDevChannel) {
    popup.webview.openDevTools();
  }

  return popupId;
}

export function openBatchPopup(batchId: string, title: string): void {
  openPopup({
    hash: `#/popup/batch-add/${encodeURIComponent(batchId)}`,
    title,
    width: 1920,
    height: 960,
    minWidth: 600,
    minHeight: 400,
  });
}

export function openAddDownloadPopup(
  addId: string,
  title: string,
  options: { fromExtensionGrab?: boolean } = {},
): number | null {
  return openPopup({
    hash: `#/popup/add-download/${encodeURIComponent(addId)}`,
    title,
    width: 560,
    height: 720,
    fromExtensionGrab: options.fromExtensionGrab,
  });
}

export function closeAllPopups(): void {
  for (const popup of popupWindows.values()) {
    popup.close();
  }
  popupWindows.clear();
}
