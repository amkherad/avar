/**
 * Headless CI check: tinytron native addon + shared desktop modules.
 * Does not open a webview window (no display server required).
 */

const fs = require("node:fs");
const path = require("node:path");
const http = require("node:http");

const { getProxyBaseUrl, startDaemonProxy, stopDaemonProxy } = require("../desktop/daemon-proxy.cjs");
const { resolveGuiUrl } = require("../desktop/resolve-gui-url.cjs");
const {
  startStaticServer,
  stopStaticServer,
} = require("../desktop/static-server.cjs");

function tinytronRoot() {
  return path.join(__dirname, "..", "node_modules", "tinytron");
}

function tinytronAddonPath() {
  return path.join(tinytronRoot(), "build", "Release", "addon.node");
}

function loadTiny() {
  const root = tinytronRoot();
  if (!fs.existsSync(path.join(root, "package.json"))) {
    throw new Error(
      "tinytron is not installed. Run: node scripts/install-tinytron.cjs",
    );
  }

  try {
    return require("tinytron");
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    throw new Error(
      `tinytron failed to load (${message}). Run: node scripts/install-tinytron.cjs`,
    );
  }
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

async function main() {
  const addonPath = tinytronAddonPath();
  if (!fs.existsSync(addonPath)) {
    throw new Error(
      `tinytron native addon not found: ${addonPath}. Run: node scripts/install-tinytron.cjs`,
    );
  }

  const Tiny = loadTiny();
  if (typeof Tiny !== "function") {
    throw new Error("tinytron did not export the Tiny constructor");
  }

  startDaemonProxy();
  const proxyUrl = getProxyBaseUrl();
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
  stopDaemonProxy();

  console.log("Tiny build verification passed");
  console.log(`  addon: ${addonPath}`);
  console.log(`  proxy: ${proxyUrl}`);
  console.log(`  static: ${staticUrl}`);
}

void main().catch((error) => {
  stopStaticServer();
  stopDaemonProxy();
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
});
