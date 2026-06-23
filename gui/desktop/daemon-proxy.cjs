/**
 * Local HTTP proxy to the Avar daemon (shared by Electron and Tiny).
 */

const http = require("node:http");
const { DAEMON_TARGET, PROXY_HOST, PROXY_PORT } = require("./env.cjs");

/** @type {import('node:http').Server | null} */
let proxyServer = null;

function getProxyBaseUrl() {
  return `http://${PROXY_HOST}:${PROXY_PORT}`;
}

function startDaemonProxy() {
  if (proxyServer) {
    return proxyServer;
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
  return proxyServer;
}

function stopDaemonProxy() {
  if (!proxyServer) {
    return;
  }
  proxyServer.close();
  proxyServer = null;
}

module.exports = {
  getProxyBaseUrl,
  startDaemonProxy,
  stopDaemonProxy,
};
