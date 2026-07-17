/**
 * Minimal static file server for the Tiny shell (production experiment).
 *
 * Serves gui/dist over HTTP from a worker thread so requests keep working
 * while the native webview blocks the main thread.
 */

const path = require("node:path");
const { Worker } = require("node:worker_threads");
const { distDir } = require("./env.cjs");
const { tinyHostInfo } = require("./host-info.cjs");

/** @type {import('node:worker_threads').Worker | null} */
let staticWorker = null;
/** @type {string | null} */
let staticServerUrl = null;

function buildHostInfoScript() {
  const payload = JSON.stringify(tinyHostInfo()).replace(/</g, "\\u003c");
  return `<script>window.__AVAR_HOST__=${payload}</script>`;
}

function startStaticServer(host = "127.0.0.1", port = 0) {
  if (staticWorker && staticServerUrl) {
    return { url: staticServerUrl };
  }

  staticWorker = new Worker(path.join(__dirname, "static-server-worker.cjs"), {
    workerData: {
      distDir,
      hostInfoScript: buildHostInfoScript(),
      host,
      port,
    },
  });

  staticWorker.on("message", (message) => {
    if (message?.type === "ready" && typeof message.url === "string") {
      staticServerUrl = message.url;
      return;
    }
    if (message?.type === "error") {
      console.error(`Static server worker failed: ${message.message ?? "unknown error"}`);
    }
  });

  staticWorker.on("error", (error) => {
    console.error(error instanceof Error ? error.message : String(error));
  });

  staticWorker.on("exit", (code) => {
    if (code !== 0 && code !== null) {
      console.error(`Static server worker exited with code ${code}`);
    }
    staticWorker = null;
    staticServerUrl = null;
  });

  return {
    get url() {
      return staticServerUrl;
    },
  };
}

function stopStaticServer() {
  if (!staticWorker) {
    return;
  }

  const worker = staticWorker;
  staticWorker = null;
  staticServerUrl = null;
  worker.postMessage({ type: "stop" });
}

module.exports = {
  startStaticServer,
  stopStaticServer,
};
