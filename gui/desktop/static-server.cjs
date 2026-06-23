/**
 * Minimal static file server for the Tiny shell (production experiment).
 */

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const { distDir } = require("./env.cjs");

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

/** @type {import('node:http').Server | null} */
let staticServer = null;
/** @type {string | null} */
let staticServerUrl = null;

function resolveDistPath(urlPath) {
  const decoded = decodeURIComponent(urlPath.split("?")[0] || "/");
  const relative = decoded === "/" ? "index.html" : decoded.replace(/^\/+/, "");
  const resolved = path.normalize(path.join(distDir, relative));

  if (!resolved.startsWith(distDir)) {
    return null;
  }

  return resolved;
}

function startStaticServer(host = "127.0.0.1", port = 0) {
  if (staticServer && staticServerUrl) {
    return { server: staticServer, url: staticServerUrl };
  }

  staticServer = http.createServer((req, res) => {
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

    res.writeHead(200, { "Content-Type": mime });
    fs.createReadStream(filePath).pipe(res);
  });

  staticServer.listen(port, host, () => {
    const address = staticServer.address();
    if (address && typeof address === "object") {
      staticServerUrl = `http://${host}:${address.port}/`;
    }
  });

  return {
    server: staticServer,
    get url() {
      return staticServerUrl;
    },
  };
}

function stopStaticServer() {
  if (!staticServer) {
    return;
  }
  staticServer.close();
  staticServer = null;
  staticServerUrl = null;
}

module.exports = {
  startStaticServer,
  stopStaticServer,
};
