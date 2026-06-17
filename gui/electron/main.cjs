const { app, BrowserWindow, ipcMain, shell } = require("electron");
const path = require("node:path");

const isDev = !app.isPackaged;
const iconPath = path.join(__dirname, "..", "icon.svg");

/** @type {Map<number, import('electron').BrowserWindow>} */
const popupWindows = new Map();
let popupCounter = 0;

function resolveAppUrl(hash = "") {
  if (isDev && process.env.VITE_DEV_SERVER_URL) {
    return `${process.env.VITE_DEV_SERVER_URL}${hash}`;
  }
  return `file://${path.join(__dirname, "..", "dist", "index.html")}${hash}`;
}

function createWindow() {
  const win = new BrowserWindow({
    width: 1200,
    height: 800,
    minWidth: 900,
    minHeight: 600,
    title: "Avar",
    icon: iconPath,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      preload: path.join(__dirname, "preload.cjs"),
    },
  });

  win.webContents.setWindowOpenHandler(({ url }) => {
    void shell.openExternal(url);
    return { action: "deny" };
  });

  if (isDev && process.env.VITE_DEV_SERVER_URL) {
    void win.loadURL(process.env.VITE_DEV_SERVER_URL);
    win.webContents.openDevTools({ mode: "detach" });
  } else {
    void win.loadFile(path.join(__dirname, "..", "dist", "index.html"));
  }
}

function createPopupWindow(options) {
  const popupId = ++popupCounter;
  const width = options.width ?? 520;
  const height = options.height ?? 640;

  const popup = new BrowserWindow({
    width,
    height,
    minWidth: 400,
    minHeight: 320,
    title: options.title ?? "Avar",
    icon: iconPath,
    parent: BrowserWindow.getFocusedWindow() ?? undefined,
    modal: false,
    autoHideMenuBar: true,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      preload: path.join(__dirname, "preload.cjs"),
    },
  });

  popupWindows.set(popupId, popup);

  popup.on("closed", () => {
    popupWindows.delete(popupId);
  });

  const targetUrl = options.url ?? resolveAppUrl();
  if (targetUrl.startsWith("http")) {
    void popup.loadURL(targetUrl);
  } else {
    void popup.loadURL(targetUrl);
  }

  return popupId;
}

ipcMain.handle("popup:open", (_event, options) => {
  return createPopupWindow(options ?? {});
});

app.whenReady().then(() => {
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});
