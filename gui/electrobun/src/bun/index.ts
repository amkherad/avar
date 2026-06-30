import Electrobun, { BrowserWindow, Updater, Utils } from "electrobun/bun";
import { createAvarRpc } from "./avar-handlers";
import {
  APP_TITLE,
  DEFAULT_WINDOW_HEIGHT,
  DEFAULT_WINDOW_WIDTH,
  resolveShellGuiUrl,
  startDaemonProxy,
  stopDaemonProxy,
} from "./desktop-shell";
import { startExtensionBridge, stopExtensionBridge } from "./extension-host";
import {
  closeAllPopups,
  configurePopupManager,
  openAddDownloadPopup,
  openBatchPopup,
} from "./popup-manager";
import { createTray, destroyTray } from "./tray-manager";
import { shouldKeepInTrayOnClose } from "./close-behavior";
import { loadAvarProtocol } from "./vendor-paths";

const SHELL_ELECTROBUN = "electrobun";
const PRELOAD_URL = "views://avarbridge/avar-preload.js";

let mainWindow: BrowserWindow | null = null;
let appIsQuitting = false;
let channel = "dev";

function shutdown(): void {
  closeAllPopups();
  stopExtensionBridge();
  stopDaemonProxy();
  destroyTray();
}

function showMainWindow(): void {
  if (!mainWindow) {
    void createMainWindow();
    return;
  }
  if (mainWindow.isMinimized()) {
    mainWindow.unminimize();
  }
  mainWindow.show();
  mainWindow.activate();
}

function requestQuit(): void {
  appIsQuitting = true;
  shutdown();
  Utils.quit();
}

function handleAvarProtocolUrl(rawUrl: string): void {
  const { parseAvarProtocolUrl, isFocusAction } = loadAvarProtocol();
  const parsed = parseAvarProtocolUrl(rawUrl);
  if (!parsed) {
    return;
  }
  if (isFocusAction(parsed.action)) {
    showMainWindow();
  }
}

function attachWebviewHandlers(win: BrowserWindow): void {
  win.webview.on("new-window-open", (event: { data: { detail: unknown } }) => {
    const detail = event.data.detail;
    const url =
      typeof detail === "string"
        ? detail
        : typeof detail === "object" &&
            detail !== null &&
            "url" in detail &&
            typeof (detail as { url: unknown }).url === "string"
          ? (detail as { url: string }).url
          : "";
    if (url) {
      Utils.openExternal(url);
    }
  });
}

async function createMainWindow(): Promise<BrowserWindow> {
  const guiUrl = await resolveShellGuiUrl(channel);
  configurePopupManager({ baseGuiUrl: guiUrl, isDevChannel: channel === "dev" });

  const win = new BrowserWindow({
    title: APP_TITLE,
    url: guiUrl,
    preload: PRELOAD_URL,
    rpc: createAvarRpc(),
    frame: {
      width: DEFAULT_WINDOW_WIDTH,
      height: DEFAULT_WINDOW_HEIGHT,
      x: 100,
      y: 100,
    },
  });

  mainWindow = win;
  attachWebviewHandlers(win);

  win.on("close", (event: { response?: { allow: boolean } }) => {
    if (appIsQuitting) {
      return;
    }
    if (shouldKeepInTrayOnClose()) {
      event.response = { allow: false };
      win.hide();
      return;
    }
    requestQuit();
  });

  if (channel === "dev") {
    win.webview.openDevTools();
  }

  return win;
}

console.log(`Avar desktop shell: ${SHELL_ELECTROBUN}`);

startDaemonProxy();

try {
  channel = await Updater.localInfo.channel();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.warn(`Could not read Electrobun channel (${message}); assuming dev.`);
}

startExtensionBridge(openBatchPopup, openAddDownloadPopup, showMainWindow);
createTray({ onShowMainWindow: showMainWindow, onQuit: requestQuit });

Electrobun.events.on("open-url", (event: { data: { url: string } }) => {
  handleAvarProtocolUrl(event.data.url);
});

Electrobun.events.on("before-quit", (event: { response?: { allow: boolean } }) => {
  if (!appIsQuitting) {
    appIsQuitting = true;
    shutdown();
  }
  event.response = { allow: true };
});

await createMainWindow();

console.log(`Loading GUI with Electrobun bridge (${channel})`);
