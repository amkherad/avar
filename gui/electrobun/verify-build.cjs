/**
 * Headless CI check: Electrobun config, dist assets, and shared desktop modules.
 * Does not open a webview window (no display server required).
 */

const fs = require("node:fs");
const path = require("node:path");
const http = require("node:http");
const net = require("node:net");

const electrobunRoot = __dirname;
const guiRoot = path.join(electrobunRoot, "..");
const distDir = path.join(guiRoot, "dist");

const { getProxyBaseUrl, startDaemonProxy, stopDaemonProxy } = require("../desktop/daemon-proxy.cjs");
const { resolveGuiUrl } = require("../desktop/resolve-gui-url.cjs");
const {
  startStaticServer,
  stopStaticServer,
} = require("../desktop/static-server.cjs");

function electrobunCliPath() {
  return path.join(guiRoot, "node_modules", "electrobun", "bin", "electrobun.cjs");
}

function isPortOpen(port, host) {
  return new Promise((resolve) => {
    const socket = net.connect({ port, host });
    socket.once("connect", () => {
      socket.end();
      resolve(true);
    });
    socket.once("error", () => resolve(false));
  });
}

async function ensureDaemonProxy() {
  const proxyUrl = getProxyBaseUrl();
  const port = Number(new URL(proxyUrl).port);
  const host = new URL(proxyUrl).hostname;

  if (await isPortOpen(port, host)) {
    return { proxyUrl, started: false };
  }

  startDaemonProxy();
  return { proxyUrl, started: true };
}

function fetchOk(url) {
  return new Promise((resolve, reject) => {
    const request = http.get(url, (response) => {
      response.resume();
      if (response.statusCode && response.statusCode >= 200 && response.statusCode < 400) {
        resolve();
        return;
      }
      reject(new Error(`Unexpected status ${response.statusCode} for ${url}`));
    });
    request.on("error", reject);
  });
}

function waitForStaticServerUrl(staticServer, timeoutMs = 10_000) {
  return new Promise((resolve, reject) => {
    const started = Date.now();
    const poll = () => {
      if (staticServer.url) {
        resolve(staticServer.url);
        return;
      }
      if (Date.now() - started > timeoutMs) {
        reject(new Error("Timed out waiting for static server"));
        return;
      }
      setTimeout(poll, 50);
    };
    poll();
  });
}

async function main() {
  if (!fs.existsSync(path.join(distDir, "index.html"))) {
    throw new Error("gui/dist/index.html is missing. Run `npm run build` first.");
  }

  if (!fs.existsSync(electrobunCliPath())) {
    throw new Error("electrobun is not installed. Run `npm install` in gui/.");
  }

  if (!fs.existsSync(path.join(electrobunRoot, "electrobun.config.ts"))) {
    throw new Error("electrobun.config.ts is missing.");
  }

  const { proxyUrl, started: proxyStarted } = await ensureDaemonProxy();
  if (!proxyUrl.includes("18765")) {
    throw new Error(`Unexpected daemon proxy URL: ${proxyUrl}`);
  }

  const fileUrl = resolveGuiUrl({ isPackaged: true });
  if (!fileUrl.startsWith("file://")) {
    throw new Error(`Expected file:// GUI URL, got: ${fileUrl}`);
  }

  const staticServer = startStaticServer();
  const staticUrl = await waitForStaticServerUrl(staticServer);
  await fetchOk(staticUrl);

  stopStaticServer();
  if (proxyStarted) {
    stopDaemonProxy();
  }

  console.log("Electrobun build verification passed");
  console.log(`  cli: ${electrobunCliPath()}`);
  console.log(`  dist: ${distDir}`);
  console.log(`  proxy: ${proxyUrl}`);
  console.log(`  static: ${staticUrl}`);
}

void main().catch((error) => {
  stopStaticServer();
  stopDaemonProxy();
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
});
