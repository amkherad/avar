const { app, BrowserWindow, ipcMain, shell, Notification } = require("electron");
const http = require("node:http");
const path = require("node:path");

const isDev = !app.isPackaged;
const iconPath = path.join(__dirname, "..", "icon.svg");
const DAEMON_TARGET = process.env.AVAR_DAEMON_URL || "http://127.0.0.1:8000";
const PROXY_HOST = "127.0.0.1";
const PROXY_PORT = Number(process.env.AVAR_ELECTRON_PROXY_PORT || 18765);

/** @type {Map<number, import('electron').BrowserWindow>} */
const popupWindows = new Map();
let popupCounter = 0;
/** @type {import('node:http').Server | null} */
let proxyServer = null;

function resolveAppUrl(hash = "") {
  if (isDev && process.env.VITE_DEV_SERVER_URL) {
    return `${process.env.VITE_DEV_SERVER_URL}${hash}`;
  }
  return `file://${path.join(__dirname, "..", "dist", "index.html")}${hash}`;
}

function startDaemonProxy() {
  if (proxyServer) {
    return;
  }

  proxyServer = http.createServer((req, res) => {
    const target = new URL(req.url || "/", DAEMON_TARGET);
    const headers = { ...req.headers, host: target.host };
    const upstream = http.request(
      {
        protocol: target.protocol,
        hostname: target.hostname,
        port: target.port,
        path: `${target.pathname}${target.search}`,
        method: req.method,
        headers,
      },
      (upstreamRes) => {
        res.writeHead(upstreamRes.statusCode || 500, upstreamRes.headers);
        upstreamRes.pipe(res);
      },
    );

    upstream.on("error", () => {
      res.statusCode = 502;
      res.end("Bad Gateway");
    });

    req.pipe(upstream);
  });

  proxyServer.listen(PROXY_PORT, PROXY_HOST);
}

function createWindow() {
  const win = new BrowserWindow({
    width: 1280,
    height: 800,
    minWidth: 1024,
    minHeight: 640,
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
  void popup.loadURL(targetUrl);

  return popupId;
}

ipcMain.handle("popup:open", (_event, options) => {
  return createPopupWindow(options ?? {});
});

ipcMain.handle("notification:show", (_event, options) => {
  if (!Notification.isSupported()) {
    return false;
  }
  const notification = new Notification({
    title: options?.title ?? "Avar",
    body: options?.body,
  });
  notification.show();
  return true;
});

ipcMain.handle("daemon:getProxyBaseUrl", () => {
  return `http://${PROXY_HOST}:${PROXY_PORT}`;
});

app.whenReady().then(() => {
  startDaemonProxy();
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

app.on("before-quit", () => {
  if (proxyServer) {
    proxyServer.close();
    proxyServer = null;
  }
});
