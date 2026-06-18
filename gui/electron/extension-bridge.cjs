/**
 * Shared browser-extension bridge handlers for Vite dev middleware and Electron.
 */

const {
  PROTOCOL_VERSION,
  BRIDGE_VERSION,
  EXTENSION_BRIDGE_HOST,
  EXTENSION_BRIDGE_PORT,
  parseEnvelope,
  createResponse,
  createErrorResponse,
} = require("./extension-protocol.cjs");

const EXTENSION_PING_TTL_MS = 30_000;

const bridgeState = {
  enabled: true,
  lastPingAt: null,
  lastExtensionVersion: null,
};

let bridgeConfig = {
  daemonUrl: process.env.AVAR_DAEMON_URL || "http://127.0.0.1:8000",
  authToken: undefined,
};

function setExtensionBridgeEnabled(enabled) {
  bridgeState.enabled = Boolean(enabled);
}

function setExtensionBridgeConfig(config) {
  bridgeConfig = {
    daemonUrl: String(config.daemonUrl).replace(/\/+$/, ""),
    authToken: config.authToken,
  };
}

function getExtensionBridgeState() {
  const connected =
    bridgeState.enabled &&
    bridgeState.lastPingAt !== null &&
    Date.now() - bridgeState.lastPingAt < EXTENSION_PING_TTL_MS;
  return {
    ...bridgeState,
    connected,
    bridgeVersion: BRIDGE_VERSION,
    protocolVersion: PROTOCOL_VERSION,
    port: EXTENSION_BRIDGE_PORT,
    host: EXTENSION_BRIDGE_HOST,
  };
}

function recordPing(extensionVersion) {
  bridgeState.lastPingAt = Date.now();
  if (extensionVersion) {
    bridgeState.lastExtensionVersion = extensionVersion;
  }
  void keepDaemonAlive();
}

async function keepDaemonAlive() {
  try {
    const headers = {};
    if (bridgeConfig.authToken) {
      headers.Authorization = `Bearer ${bridgeConfig.authToken}`;
    }
    await fetch(`${bridgeConfig.daemonUrl}/api/ping`, { headers });
  } catch {
    // Daemon may be stopped while the bridge is still running.
  }
}

function corsHeaders(origin) {
  return {
    "Access-Control-Allow-Origin": origin ?? "*",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type, Authorization",
  };
}

function sendJson(res, status, body, origin) {
  const headers = corsHeaders(origin);
  for (const [key, value] of Object.entries(headers)) {
    res.setHeader(key, value);
  }
  res.setHeader("Content-Type", "application/json");
  res.statusCode = status;
  res.end(JSON.stringify(body));
}

async function readBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    req.on("data", (chunk) => chunks.push(chunk));
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", reject);
  });
}

async function daemonRpc(method, params) {
  const headers = { "Content-Type": "application/json" };
  if (bridgeConfig.authToken) {
    headers.Authorization = `Bearer ${bridgeConfig.authToken}`;
  }

  const response = await fetch(`${bridgeConfig.daemonUrl}/api/rpc`, {
    method: "POST",
    headers,
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

async function handleDownloadAdd(payload) {
  const params = { url: payload.url.trim(), attached: false };
  if (typeof payload.streamKind === "string" && payload.streamKind) {
    params.streamKind = payload.streamKind;
  }
  if (typeof payload.referer === "string" && payload.referer) {
    params.referer = payload.referer;
  }
  const result = await daemonRpc("download.add", params);
  const exitCode = result?.exitCode;
  if (exitCode !== undefined && exitCode !== 0) {
    throw new Error("download.add failed");
  }
  recordPing();
}

async function handleProtocolMessage(message, origin, res) {
  const { type, id, payload } = message;

  if (type === "ping") {
    recordPing(typeof payload.extensionVersion === "string" ? payload.extensionVersion : undefined);
    sendJson(
      res,
      200,
      createResponse("ping", id, {
        bridgeVersion: BRIDGE_VERSION,
        protocolVersion: PROTOCOL_VERSION,
      }),
      origin,
    );
    return;
  }

  if (type === "status") {
    sendJson(res, 200, createResponse("status", id, getExtensionBridgeState()), origin);
    return;
  }

  if (type === "download.add") {
    try {
      const url = typeof payload.url === "string" ? payload.url : "";
      if (!url.trim()) {
        sendJson(res, 400, createErrorResponse("download.add", id, "Missing url"), origin);
        return;
      }
      await handleDownloadAdd(payload);
      sendJson(res, 200, createResponse("download.add", id, {}), origin);
    } catch (error) {
      sendJson(
        res,
        502,
        createErrorResponse(
          "download.add",
          id,
          error instanceof Error ? error.message : "Request failed",
        ),
        origin,
      );
    }
    return;
  }

  sendJson(res, 404, createErrorResponse(type, id, `Unknown message type: ${type}`), origin);
}

async function handleExtensionBridgeRequest(req, res) {
  const urlPath = (req.url ?? "/").split("?")[0];
  const origin = req.headers.origin;

  if (!urlPath.startsWith("/extension") && !urlPath.startsWith("/v1")) {
    return false;
  }

  if (req.method === "OPTIONS") {
    const headers = corsHeaders(origin);
    for (const [key, value] of Object.entries(headers)) {
      res.setHeader(key, value);
    }
    res.statusCode = 204;
    res.end();
    return true;
  }

  if (!bridgeState.enabled) {
    sendJson(res, 503, { ok: false, error: "Browser extension bridge is disabled in Avar settings." }, origin);
    return true;
  }

  if (req.method === "GET" && urlPath === "/v1/ping") {
    recordPing();
    sendJson(
      res,
      200,
      {
        ok: true,
        protocol: "avar.extension",
        version: PROTOCOL_VERSION,
        bridgeVersion: BRIDGE_VERSION,
      },
      origin,
    );
    return true;
  }

  if (req.method === "POST" && urlPath === "/v1") {
    try {
      const raw = await readBody(req);
      const body = JSON.parse(raw || "{}");
      const parsed = parseEnvelope(body);
      if (!parsed.ok) {
        sendJson(res, 400, { ok: false, error: parsed.error }, origin);
        return true;
      }
      await handleProtocolMessage(parsed.message, origin, res);
    } catch (error) {
      sendJson(
        res,
        400,
        { ok: false, error: error instanceof Error ? error.message : "Invalid message" },
        origin,
      );
    }
    return true;
  }

  if (req.method === "GET" && urlPath === "/extension/ping") {
    recordPing();
    sendJson(res, 200, { ok: true, version: BRIDGE_VERSION, protocolVersion: PROTOCOL_VERSION }, origin);
    return true;
  }

  if (req.method === "GET" && urlPath === "/extension/status") {
    sendJson(res, 200, getExtensionBridgeState(), origin);
    return true;
  }

  if (req.method === "POST" && urlPath === "/extension/settings") {
    try {
      const raw = await readBody(req);
      const body = JSON.parse(raw || "{}");
      if (typeof body.enabled === "boolean") {
        setExtensionBridgeEnabled(body.enabled);
      }
      if (body.daemonUrl) {
        setExtensionBridgeConfig({
          daemonUrl: body.daemonUrl,
          authToken: body.authToken,
        });
      }
      sendJson(res, 200, { ok: true }, origin);
    } catch (error) {
      sendJson(
        res,
        400,
        { ok: false, error: error instanceof Error ? error.message : "Invalid settings" },
        origin,
      );
    }
    return true;
  }

  if (req.method === "POST" && urlPath === "/extension/download") {
    try {
      const raw = await readBody(req);
      const body = JSON.parse(raw || "{}");
      if (!body.url?.trim()) {
        sendJson(res, 400, { ok: false, error: "Missing url" }, origin);
        return true;
      }
      await handleDownloadAdd({ url: body.url });
      sendJson(res, 200, { ok: true }, origin);
    } catch (error) {
      sendJson(
        res,
        502,
        { ok: false, error: error instanceof Error ? error.message : "Request failed" },
        origin,
      );
    }
    return true;
  }

  sendJson(res, 404, { ok: false, error: "Not found" }, origin);
  return true;
}

function createExtensionBridgeServer() {
  const http = require("node:http");
  return http.createServer((req, res) => {
    void handleExtensionBridgeRequest(req, res).then((handled) => {
      if (!handled) {
        res.statusCode = 404;
        res.end("Not found");
      }
    });
  });
}

module.exports = {
  handleExtensionBridgeRequest,
  setExtensionBridgeEnabled,
  setExtensionBridgeConfig,
  getExtensionBridgeState,
  createExtensionBridgeServer,
  DEFAULT_EXTENSION_GUI_URL: "http://127.0.0.1:5173",
  ELECTRON_EXTENSION_BRIDGE_URL: `http://${EXTENSION_BRIDGE_HOST}:${EXTENSION_BRIDGE_PORT}`,
  EXTENSION_BRIDGE_HOST,
  EXTENSION_BRIDGE_PORT,
  BRIDGE_VERSION,
  BUNDLED_EXTENSION_VERSION: "0.1.0",
};
