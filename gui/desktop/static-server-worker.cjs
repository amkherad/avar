/**
 * Static file server worker for the Tiny shell.
 *
 * Runs on a separate thread so HTTP keeps responding while the native
 * webview blocks the main thread with a Win32 message loop.
 */

const { parentPort, workerData } = require("node:worker_threads");
const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const MIME_TYPES = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".webp": "image/webp",
  ".woff2": "font/woff2",
  ".webmanifest": "application/manifest+json",
};

const distDir = path.resolve(workerData.distDir);
const hostInfoScript = workerData.hostInfoScript;
const host = workerData.host || "127.0.0.1";
const port = workerData.port || 0;

function resolveDistPath(urlPath) {
  const decoded = decodeURIComponent(urlPath.split("?")[0] || "/");
  const relative = decoded === "/" ? "index.html" : decoded.replace(/^\/+/, "");
  const resolved = path.normalize(path.join(distDir, relative));
  const relativeToDist = path.relative(distDir, resolved);

  if (relativeToDist.startsWith("..") || path.isAbsolute(relativeToDist)) {
    return null;
  }

  return resolved;
}

function injectHostInfo(html) {
  if (html.includes("</head>")) {
    return html.replace("</head>", `${hostInfoScript}</head>`);
  }
  return `${hostInfoScript}${html}`;
}

const server = http.createServer((req, res) => {
  let filePath = resolveDistPath(req.url || "/");
  if (!filePath) {
    res.statusCode = 403;
    res.end("Forbidden");
    return;
  }

  if (!fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
    filePath = path.join(distDir, "index.html");
  }

  const ext = path.extname(filePath).toLowerCase();
  const mime = MIME_TYPES[ext] || "application/octet-stream";
  const headers = {
    "Content-Type": mime,
    "Cache-Control": "no-cache",
  };

  if (ext === ".html") {
    let html = fs.readFileSync(filePath, "utf8");
    if (path.basename(filePath) === "index.html") {
      html = injectHostInfo(html);
    }
    res.writeHead(200, headers);
    res.end(html);
    return;
  }

  res.writeHead(200, headers);
  fs.createReadStream(filePath).pipe(res);
});

server.listen(port, host, () => {
  const address = server.address();
  if (!address || typeof address !== "object") {
    parentPort.postMessage({ type: "error", message: "Failed to bind static server" });
    return;
  }

  parentPort.postMessage({
    type: "ready",
    url: `http://${host}:${address.port}/`,
  });
});

parentPort.on("message", (message) => {
  if (message?.type === "stop") {
    server.close(() => {
      parentPort.postMessage({ type: "stopped" });
      process.exit(0);
    });
  }
});

server.on("error", (error) => {
  parentPort.postMessage({
    type: "error",
    message: error instanceof Error ? error.message : String(error),
  });
});
