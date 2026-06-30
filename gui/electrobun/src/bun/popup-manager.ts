import { BrowserWindow } from "electrobun/bun";
import type { AvarPopupOptions } from "../../shared/avar-rpc";
import { APP_TITLE } from "./desktop-shell";
import { createAvarRpc } from "./avar-handlers";
import { loadResolveGuiUrl } from "./vendor-paths";

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

export function openPopup(options: AvarPopupOptions = {}): number {
  const popupId = ++popupCounter;
  const width = options.width ?? 520;
  const height = options.height ?? 640;
  const url = resolvePopupUrl(options);

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
  });

  popupWindows.set(popupId, popup);

  popup.on("close", () => {
    popupWindows.delete(popupId);
  });

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

export function openAddDownloadPopup(addId: string, title: string): void {
  openPopup({
    hash: `#/popup/add-download/${encodeURIComponent(addId)}`,
    title,
    width: 560,
    height: 720,
  });
}

export function closeAllPopups(): void {
  for (const popup of popupWindows.values()) {
    popup.close();
  }
  popupWindows.clear();
}
