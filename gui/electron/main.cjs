const {
  app,
  BrowserWindow,
  ipcMain,
  shell,
  Notification,
  Tray,
  Menu,
  nativeImage,
} = require("electron");
const http = require("node:http");
const path = require("node:path");
const {
  setExtensionBridgeEnabled,
  setExtensionBridgeConfig,
  getExtensionBridgeState,
  createExtensionBridgeServer,
  ELECTRON_EXTENSION_BRIDGE_URL,
  EXTENSION_BRIDGE_HOST,
  EXTENSION_BRIDGE_PORT,
} = require("./extension-bridge.cjs");
const { trayMenuIcon } = require("./tray-menu-icons.cjs");

const isDev = !app.isPackaged;

function fsExists(filePath) {
  try {
    require("node:fs").accessSync(filePath);
    return true;
  } catch {
    return false;
  }
}

const iconSvgPath = path.join(__dirname, "..", "icon.svg");
const iconPngPath = path.join(__dirname, "icon-16.png");
const iconPngFallback = path.join(__dirname, "..", "public", "icon-128.png");
const trayIconPath = fsExists(iconPngPath)
  ? iconPngPath
  : fsExists(iconPngFallback)
    ? iconPngFallback
    : iconSvgPath;

function loadAppIcon() {
  const image = nativeImage.createFromPath(trayIconPath);
  if (!image.isEmpty()) {
    return image;
  }
  return nativeImage.createFromPath(iconSvgPath);
}

const DAEMON_TARGET = process.env.AVAR_DAEMON_URL || "http://127.0.0.1:8000";
const PROXY_HOST = "127.0.0.1";
const PROXY_PORT = Number(process.env.AVAR_ELECTRON_PROXY_PORT || 18765);

/** @type {Map<number, import('electron').BrowserWindow>} */
const popupWindows = new Map();
let popupCounter = 0;
/** @type {import('node:http').Server | null} */
let proxyServer = null;
/** @type {import('node:http').Server | null} */
let extensionBridgeServer = null;
/** @type {import('electron').Tray | null} */
let tray = null;
/** @type {import('electron').BrowserWindow | null} */
let mainWindow = null;
let appIsQuitting = false;

const trayLabels = {
  show: "Show Avar",
  exit: "Exit Avar",
  startAll: "Start All",
  pauseAll: "Pause All",
  resumeAll: "Resume All",
  stopAll: "Stop All",
};

/** @type {{ sectionLabel: string, items: Array<{ id: string, filename: string, percent: number }> }} */
let trayActiveDownloads = { sectionLabel: "Active downloads", items: [] };

function resolveAppUrl(hash = "") {
  if (isDev && process.env.VITE_DEV_SERVER_URL) {
    return `${process.env.VITE_DEV_SERVER_URL}${hash}`;
  }
  return `file://${path.join(__dirname, "..", "dist", "index.html")}${hash}`;
}

async function daemonRpc(method, params) {
  const response = await fetch(`${DAEMON_TARGET}/api/rpc`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ jsonrpc: "2.0", method, params, id: Date.now() }),
  });
  if (!response.ok) {
    throw new Error(`Daemon HTTP ${response.status}`);
  }
  const payload = await response.json();
  if (payload.error) {
    throw new Error(payload.error.message || "RPC error");
  }
  return payload.result;
}

async function listDownloads() {
  const result = await daemonRpc("cli.exec", {
    argv: ["avar", "config", "get", "dm.items", "--format=json"],
  });
  if (result?.exitCode !== 0 || !result.stdout) {
    return [];
  }
  try {
    const parsed = JSON.parse(result.stdout);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

function canStart(status) {
  return ["queued", "stopped", "error", "failed", "cancelled"].includes(status);
}

function canPause(status) {
  return status === "downloading";
}

function canResume(status) {
  return status === "paused";
}

function canStop(status) {
  return ["downloading", "paused", "queued"].includes(status);
}

async function runBulkAction(kind) {
  const downloads = await listDownloads();
  const ids = downloads
    .filter((item) => {
      if (kind === "start") {
        return canStart(item.status);
      }
      if (kind === "pause") {
        return canPause(item.status);
      }
      if (kind === "resume") {
        return canResume(item.status);
      }
      if (kind === "stop") {
        return canStop(item.status);
      }
      return false;
    })
    .map((item) => item.id)
    .filter(Boolean);

  for (const id of ids) {
    try {
      if (kind === "start") {
        await daemonRpc("cli.exec", { argv: ["avar", "download", "start", id] });
      } else if (kind === "pause") {
        await daemonRpc("cli.exec", { argv: ["avar", "download", "pause", id] });
      } else if (kind === "resume") {
        await daemonRpc("cli.exec", { argv: ["avar", "download", "resume", id] });
      } else if (kind === "stop") {
        await daemonRpc("cli.exec", { argv: ["avar", "download", "stop", id] });
      }
    } catch {
      // Continue with remaining downloads.
    }
  }
}

function showMainWindow() {
  if (!mainWindow) {
    createWindow();
    return;
  }
  if (mainWindow.isMinimized()) {
    mainWindow.restore();
  }
  mainWindow.show();
  mainWindow.focus();
}

function buildTrayMenu() {
  const showIcon = loadAppIcon().resize({ width: 16, height: 16 });
  /** @type {import('electron').MenuItemConstructorOptions[]} */
  const template = [
    {
      label: trayLabels.show,
      icon: showIcon.isEmpty() ? trayMenuIcon("show") : showIcon,
      click: () => showMainWindow(),
    },
    { type: "separator" },
  ];

  if (trayActiveDownloads.items.length > 0) {
    template.push({ label: trayActiveDownloads.sectionLabel, enabled: false });
    for (const item of trayActiveDownloads.items) {
      const label =
        item.filename.length > 48 ? `${item.filename.slice(0, 47)}…` : item.filename;
      template.push({
        label: `${label} (${item.percent}%)`,
        icon: trayMenuIcon("download"),
        enabled: false,
      });
    }
    template.push({ type: "separator" });
  }

  template.push(
    {
      label: trayLabels.startAll,
      icon: trayMenuIcon("play"),
      click: () => void runBulkAction("start"),
    },
    {
      label: trayLabels.pauseAll,
      icon: trayMenuIcon("pause"),
      click: () => void runBulkAction("pause"),
    },
    {
      label: trayLabels.resumeAll,
      icon: trayMenuIcon("resume"),
      click: () => void runBulkAction("resume"),
    },
    {
      label: trayLabels.stopAll,
      icon: trayMenuIcon("stop"),
      click: () => void runBulkAction("stop"),
    },
    { type: "separator" },
    {
      label: trayLabels.exit,
      icon: trayMenuIcon("exit"),
      click: () => {
        appIsQuitting = true;
        app.quit();
      },
    },
  );

  return Menu.buildFromTemplate(template);
}

function updateTrayTooltip() {
  if (!tray) {
    return;
  }

  if (trayActiveDownloads.items.length === 0) {
    tray.setToolTip("Avar");
    return;
  }

  const lines = trayActiveDownloads.items.map(
    (item) => `${item.filename} (${item.percent}%)`,
  );
  tray.setToolTip(["Avar", ...lines].join("\n"));
}

function createTray() {
  if (tray) {
    return;
  }

  const icon = loadAppIcon();
  const trayImage = icon.isEmpty() ? icon : icon.resize({ width: 16, height: 16 });
  tray = new Tray(trayImage);
  tray.setToolTip("Avar");
  tray.setContextMenu(buildTrayMenu());
  updateTrayTooltip();

  tray.on("click", () => {
    showMainWindow();
  });
}

function startExtensionBridge() {
  if (extensionBridgeServer) {
    return;
  }

  extensionBridgeServer = createExtensionBridgeServer();
  extensionBridgeServer.listen(EXTENSION_BRIDGE_PORT, EXTENSION_BRIDGE_HOST);
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
    icon: loadAppIcon(),
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      preload: path.join(__dirname, "preload.cjs"),
    },
  });

  mainWindow = win;

  win.on("close", (event) => {
    if (!appIsQuitting) {
      event.preventDefault();
      win.hide();
    }
  });

  win.on("closed", () => {
    if (mainWindow === win) {
      mainWindow = null;
    }
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
    icon: loadAppIcon(),
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

ipcMain.handle("extensionBridge:setEnabled", (_event, enabled) => {
  setExtensionBridgeEnabled(Boolean(enabled));
});

ipcMain.handle("extensionBridge:setConfig", (_event, config) => {
  if (config?.daemonUrl) {
    setExtensionBridgeConfig({
      daemonUrl: config.daemonUrl,
      authToken: config.authToken,
    });
  }
});

ipcMain.handle("extensionBridge:getState", () => {
  return getExtensionBridgeState();
});

ipcMain.handle("extensionBridge:getUrl", () => {
  return ELECTRON_EXTENSION_BRIDGE_URL;
});

ipcMain.handle("tray:setLabels", (_event, labels) => {
  if (labels && typeof labels === "object") {
    Object.assign(trayLabels, labels);
    if (tray) {
      tray.setContextMenu(buildTrayMenu());
    }
  }
});

ipcMain.handle("tray:setActiveDownloads", (_event, payload) => {
  if (payload && typeof payload === "object") {
    trayActiveDownloads = {
      sectionLabel:
        typeof payload.sectionLabel === "string"
          ? payload.sectionLabel
          : trayActiveDownloads.sectionLabel,
      items: Array.isArray(payload.items)
        ? payload.items
            .filter(
              (item) =>
                item &&
                typeof item.id === "string" &&
                typeof item.filename === "string" &&
                typeof item.percent === "number",
            )
            .slice(0, 3)
        : [],
    };
    if (tray) {
      tray.setContextMenu(buildTrayMenu());
      updateTrayTooltip();
    }
  }
});

app.whenReady().then(() => {
  startExtensionBridge();
  startDaemonProxy();
  createWindow();
  createTray();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    } else {
      showMainWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (tray) {
    return;
  }
  if (process.platform !== "darwin") {
    app.quit();
  }
});

app.on("before-quit", () => {
  if (extensionBridgeServer) {
    extensionBridgeServer.close();
    extensionBridgeServer = null;
  }
  if (proxyServer) {
    proxyServer.close();
    proxyServer = null;
  }
  if (tray) {
    tray.destroy();
    tray = null;
  }
});
