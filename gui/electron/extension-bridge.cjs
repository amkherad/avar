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
const { EXTENSION_GUI_URL } = require("../dev-server.cjs");

const EXTENSION_PING_TTL_MS = 30_000;
const BATCH_STASH_TTL_MS = 10 * 60 * 1000;

const bridgeState = {
  enabled: true,
  lastPingAt: null,
  lastExtensionVersion: null,
};

/** @type {Map<string, { createdAt: number, payload: Record<string, unknown> }>} */
const batchStash = new Map();

/** @type {Map<string, { createdAt: number, payload: Record<string, unknown> }>} */
const addDownloadStash = new Map();

/** @type {((batchId: string, title: string) => void) | null} */
let batchPopupOpener = null;

/** @type {((addId: string, title: string) => void) | null} */
let addDownloadPopupOpener = null;

/** @type {(() => void) | null} */
let appFocusHandler = null;

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

function setBatchPopupOpener(opener) {
  batchPopupOpener = typeof opener === "function" ? opener : null;
}

function setAddDownloadPopupOpener(opener) {
  addDownloadPopupOpener = typeof opener === "function" ? opener : null;
}

function setAppFocusHandler(handler) {
  appFocusHandler = typeof handler === "function" ? handler : null;
}

function focusAvarApp() {
  if (!appFocusHandler) {
    return;
  }
  try {
    appFocusHandler();
  } catch {
    // Ignore focus failures.
  }
}

function pruneBatchStash() {
  const now = Date.now();
  for (const [id, entry] of batchStash.entries()) {
    if (now - entry.createdAt > BATCH_STASH_TTL_MS) {
      batchStash.delete(id);
    }
  }
  for (const [id, entry] of addDownloadStash.entries()) {
    if (now - entry.createdAt > BATCH_STASH_TTL_MS) {
      addDownloadStash.delete(id);
    }
  }
}

function stashBatchPayload(payload) {
  pruneBatchStash();
  const batchId = `batch-${Date.now()}-${Math.random().toString(36).slice(2, 9)}`;
  batchStash.set(batchId, { createdAt: Date.now(), payload });
  return batchId;
}

function stashAddDownloadPayload(payload) {
  pruneBatchStash();
  const addId = `add-${Date.now()}-${Math.random().toString(36).slice(2, 9)}`;
  addDownloadStash.set(addId, { createdAt: Date.now(), payload });
  return addId;
}

function readBatchPayload(batchId) {
  const entry = batchStash.get(batchId);
  if (!entry) {
    return null;
  }
  if (Date.now() - entry.createdAt > BATCH_STASH_TTL_MS) {
    batchStash.delete(batchId);
    return null;
  }
  return entry.payload;
}

function readAddDownloadPayload(addId) {
  const entry = addDownloadStash.get(addId);
  if (!entry) {
    return null;
  }
  if (Date.now() - entry.createdAt > BATCH_STASH_TTL_MS) {
    addDownloadStash.delete(addId);
    return null;
  }
  return entry.payload;
}

function normalizeAddDownloadPayload(payload, pageUrl) {
  const url = typeof payload.url === "string" ? payload.url.trim() : "";
  if (!url) {
    return null;
  }

  const referer =
    typeof payload.referer === "string" && payload.referer.trim()
      ? payload.referer.trim()
      : typeof pageUrl === "string" && pageUrl.trim()
        ? pageUrl.trim()
        : undefined;

  return {
    url,
    filename:
      typeof payload.filename === "string" && payload.filename.trim()
        ? payload.filename.trim()
        : undefined,
    referer,
    streamKind:
      typeof payload.streamKind === "string" && payload.streamKind.trim()
        ? payload.streamKind.trim()
        : undefined,
    defaultQueueId:
      typeof payload.defaultQueueId === "string" ? payload.defaultQueueId : null,
    pageTitle:
      typeof payload.pageTitle === "string" && payload.pageTitle.trim()
        ? payload.pageTitle.trim()
        : undefined,
  };
}

function normalizeBatchItems(items, pageUrl) {
  if (!Array.isArray(items)) {
    return [];
  }

  const seen = new Set();
  const normalized = [];

  for (const [index, raw] of items.entries()) {
    if (!raw || typeof raw !== "object") {
      continue;
    }
    const url = typeof raw.url === "string" ? raw.url.trim() : "";
    if (!url || seen.has(url)) {
      continue;
    }
    seen.add(url);

    const fileSize =
      typeof raw.fileSize === "number" && raw.fileSize >= 0 ? raw.fileSize : null;

    normalized.push({
      id: typeof raw.id === "string" && raw.id.trim() ? raw.id.trim() : url || `item-${index}`,
      url,
      filename: typeof raw.filename === "string" ? raw.filename.trim() : undefined,
      linkName: typeof raw.linkName === "string" ? raw.linkName.trim() : undefined,
      fileType:
        typeof raw.fileType === "string"
          ? raw.fileType.trim()
          : typeof raw.streamKind === "string"
            ? raw.streamKind.trim()
            : undefined,
      originalUrl:
        typeof raw.originalUrl === "string"
          ? raw.originalUrl.trim()
          : typeof raw.referer === "string"
            ? raw.referer.trim()
            : typeof pageUrl === "string"
              ? pageUrl.trim()
              : undefined,
      referer:
        typeof raw.referer === "string"
          ? raw.referer.trim()
          : typeof raw.originalUrl === "string"
            ? raw.originalUrl.trim()
            : typeof pageUrl === "string"
              ? pageUrl.trim()
              : undefined,
      fileSize,
      streamKind:
        typeof raw.streamKind === "string"
          ? raw.streamKind.trim()
          : typeof raw.fileType === "string"
            ? raw.fileType.trim()
            : undefined,
    });
  }

  return normalized;
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
  if (typeof payload.queue === "string" && payload.queue) {
    params.queue = payload.queue;
  }
  if (payload.autoStart !== false || payload.startNow) {
    params.startNow = true;
  }
  const result = await daemonRpc("download.add", params);
  const exitCode = result?.exitCode;
  if (exitCode !== undefined && exitCode !== 0) {
    throw new Error("download.add failed");
  }
  recordPing();
  return result;
}

async function probeRemoteUrl(url, referer) {
  const headers = {};
  if (typeof referer === "string" && referer) {
    headers.Referer = referer;
  }

  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 10_000);
  try {
    let response = await fetch(url, {
      method: "HEAD",
      headers,
      redirect: "follow",
      signal: controller.signal,
    });

    if (response.status === 405 || response.status === 501) {
      response = await fetch(url, {
        method: "GET",
        headers: { ...headers, Range: "bytes=0-0" },
        redirect: "follow",
        signal: controller.signal,
      });
    }

    const result = { size: null, filename: null };

    const contentDisposition = response.headers.get("content-disposition");
    if (contentDisposition) {
      const starMatch = /filename\*\s*=\s*([^;]+)/i.exec(contentDisposition);
      if (starMatch) {
        let encoded = starMatch[1].trim();
        if (encoded.toLowerCase().startsWith("utf-8''")) {
          encoded = encoded.slice(7);
        }
        encoded = encoded.replace(/^["']|["']$/g, "");
        try {
          result.filename = decodeURIComponent(encoded);
        } catch {
          result.filename = encoded;
        }
      } else {
        const match = /filename\s*=\s*([^;]+)/i.exec(contentDisposition);
        if (match) {
          result.filename = match[1].trim().replace(/^["']|["']$/g, "");
        }
      }
    }

    if (!result.filename) {
      for (const name of [
        "x-filename",
        "x-file-name",
        "x-suggested-filename",
        "x-download-filename",
      ]) {
        const value = response.headers.get(name);
        if (value?.trim()) {
          result.filename = value.trim().replace(/^["']|["']$/g, "");
          break;
        }
      }
    }

    if (!result.filename) {
      const contentLocation = response.headers.get("content-location");
      if (contentLocation) {
        try {
          const segment = new URL(contentLocation, url).pathname.split("/").filter(Boolean).pop();
          if (segment) {
            result.filename = decodeURIComponent(segment);
          }
        } catch {
          // Ignore malformed content-location values.
        }
      }
    }

    const contentLength = response.headers.get("content-length");
    if (contentLength) {
      const size = Number(contentLength);
      if (Number.isFinite(size) && size >= 0) {
        result.size = size;
      }
    }

    if (result.size === null) {
      const contentRange = response.headers.get("content-range");
      if (contentRange) {
        const match = /\/(\d+)\s*$/i.exec(contentRange);
        if (match) {
          const size = Number(match[1]);
          if (Number.isFinite(size) && size >= 0) {
            result.size = size;
          }
        }
      }
    }

    return result;
  } catch {
    return { size: null, filename: null };
  } finally {
    clearTimeout(timeout);
  }
}

function resolvePlaylistUrl(raw, baseUrl) {
  try {
    return new URL(raw.trim(), baseUrl).href;
  } catch {
    return null;
  }
}

function formatHlsVariantLabel(resolution, bandwidth) {
  if (resolution) {
    const parts = resolution.split("x");
    const height = Number(parts[1]);
    if (Number.isFinite(height) && height > 0) {
      return `${height}p`;
    }
    return resolution;
  }
  if (bandwidth) {
    const mbps = Number(bandwidth) / 1_000_000;
    if (Number.isFinite(mbps) && mbps >= 0.1) {
      return `${mbps.toFixed(1)} Mbps`;
    }
  }
  return "HLS";
}

function parseHlsMasterPlaylist(text, baseUrl) {
  const variants = [];
  const lines = text.split(/\r?\n/);

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line.startsWith("#EXT-X-STREAM-INF:")) {
      continue;
    }

    const resolution = /RESOLUTION=(\d+x\d+)/i.exec(line)?.[1] ?? null;
    const bandwidthMatch = /BANDWIDTH=(\d+)/i.exec(line);
    const bandwidth = bandwidthMatch ? Number(bandwidthMatch[1]) : null;

    let uri = "";
    for (let j = i + 1; j < lines.length; j++) {
      const next = lines[j].trim();
      if (!next || next.startsWith("#")) {
        continue;
      }
      uri = next;
      break;
    }
    if (!uri) {
      continue;
    }

    const resolved = resolvePlaylistUrl(uri, baseUrl);
    if (!resolved) {
      continue;
    }

    variants.push({
      url: resolved,
      resolution,
      bandwidth: Number.isFinite(bandwidth) ? bandwidth : null,
      label: formatHlsVariantLabel(resolution, bandwidth),
    });
  }

  return variants;
}

async function fetchPlaylistText(url, referer) {
  const headers = {};
  if (typeof referer === "string" && referer) {
    headers.Referer = referer;
  }

  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 15_000);
  try {
    const response = await fetch(url, {
      headers,
      redirect: "follow",
      signal: controller.signal,
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    return await response.text();
  } finally {
    clearTimeout(timeout);
  }
}

async function listHlsVariants(url, referer) {
  const text = await fetchPlaylistText(url, referer);
  if (text.includes("#EXT-X-STREAM-INF")) {
    const variants = parseHlsMasterPlaylist(text, url);
    if (variants.length > 0) {
      return { variants, master: true };
    }
  }

  return {
    variants: [{ url, resolution: null, bandwidth: null, label: "HLS" }],
    master: false,
  };
}

async function handleQueueList() {
  const result = await daemonRpc("queue.list", {});
  const exitCode = result?.exitCode;
  if (exitCode !== undefined && exitCode !== 0) {
    throw new Error("queue.list failed");
  }
  return Array.isArray(result?.queues) ? result.queues : [];
}

async function handleQueueControl(action, queueId) {
  const method = action === "start" ? "queue.start" : "queue.stop";
  const result = await daemonRpc(method, { id: queueId });
  const exitCode = result?.exitCode;
  if (exitCode !== undefined && exitCode !== 0) {
    throw new Error(`${method} failed`);
  }
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
      focusAvarApp();
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

  if (type === "download.add.open") {
    try {
      const pageUrl = typeof payload.pageUrl === "string" ? payload.pageUrl : undefined;
      const addPayload = normalizeAddDownloadPayload(payload, pageUrl);
      if (!addPayload) {
        sendJson(res, 400, createErrorResponse("download.add.open", id, "Missing url"), origin);
        return;
      }

      const addId = stashAddDownloadPayload(addPayload);
      const title =
        typeof payload.title === "string" && payload.title.trim()
          ? payload.title.trim()
          : "Add download";

      focusAvarApp();
      if (addDownloadPopupOpener) {
        addDownloadPopupOpener(addId, title);
      }

      sendJson(
        res,
        200,
        createResponse("download.add.open", id, { addId, title }),
        origin,
      );
    } catch (error) {
      sendJson(
        res,
        502,
        createErrorResponse(
          "download.add.open",
          id,
          error instanceof Error ? error.message : "Request failed",
        ),
        origin,
      );
    }
    return;
  }

  if (type === "download.batch.open") {
    try {
      const pageUrl = typeof payload.pageUrl === "string" ? payload.pageUrl : undefined;
      const items = normalizeBatchItems(payload.items, pageUrl);
      if (items.length === 0) {
        sendJson(res, 400, createErrorResponse("download.batch.open", id, "No items"), origin);
        return;
      }

      const batchPayload = {
        items,
        defaultQueueId:
          typeof payload.defaultQueueId === "string" ? payload.defaultQueueId : null,
        pageTitle: typeof payload.pageTitle === "string" ? payload.pageTitle : undefined,
      };
      const batchId = stashBatchPayload(batchPayload);
      const title =
        typeof payload.title === "string" && payload.title.trim()
          ? payload.title.trim()
          : "Add downloads";

      focusAvarApp();
      if (batchPopupOpener) {
        batchPopupOpener(batchId, title);
      }

      sendJson(
        res,
        200,
        createResponse("download.batch.open", id, { batchId, title }),
        origin,
      );
    } catch (error) {
      sendJson(
        res,
        502,
        createErrorResponse(
          "download.batch.open",
          id,
          error instanceof Error ? error.message : "Request failed",
        ),
        origin,
      );
    }
    return;
  }

  if (type === "url.probe") {
    try {
      const url = typeof payload.url === "string" ? payload.url : "";
      if (!url.trim()) {
        sendJson(res, 400, createErrorResponse("url.probe", id, "Missing url"), origin);
        return;
      }
      const referer = typeof payload.referer === "string" ? payload.referer : undefined;
      const result = await probeRemoteUrl(url.trim(), referer);
      sendJson(res, 200, createResponse("url.probe", id, result), origin);
    } catch (error) {
      sendJson(
        res,
        502,
        createErrorResponse(
          "url.probe",
          id,
          error instanceof Error ? error.message : "Request failed",
        ),
        origin,
      );
    }
    return;
  }

  if (type === "hls.list") {
    try {
      const url = typeof payload.url === "string" ? payload.url : "";
      if (!url.trim()) {
        sendJson(res, 400, createErrorResponse("hls.list", id, "Missing url"), origin);
        return;
      }
      const referer = typeof payload.referer === "string" ? payload.referer : undefined;
      const result = await listHlsVariants(url.trim(), referer);
      sendJson(res, 200, createResponse("hls.list", id, result), origin);
    } catch (error) {
      sendJson(
        res,
        502,
        createErrorResponse(
          "hls.list",
          id,
          error instanceof Error ? error.message : "Request failed",
        ),
        origin,
      );
    }
    return;
  }

  if (type === "queue.list") {
    try {
      const queues = await handleQueueList();
      sendJson(res, 200, createResponse("queue.list", id, { queues }), origin);
    } catch (error) {
      sendJson(
        res,
        502,
        createErrorResponse(
          "queue.list",
          id,
          error instanceof Error ? error.message : "Request failed",
        ),
        origin,
      );
    }
    return;
  }

  if (type === "queue.start" || type === "queue.stop") {
    try {
      const queueId = typeof payload.id === "string" ? payload.id : "";
      if (!queueId) {
        sendJson(res, 400, createErrorResponse(type, id, "Missing queue id"), origin);
        return;
      }
      await handleQueueControl(type === "queue.start" ? "start" : "stop", queueId);
      sendJson(res, 200, createResponse(type, id, {}), origin);
    } catch (error) {
      sendJson(
        res,
        502,
        createErrorResponse(type, id, error instanceof Error ? error.message : "Request failed"),
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

  if (req.method === "GET" && urlPath.startsWith("/extension/batch/")) {
    const batchId = decodeURIComponent(urlPath.slice("/extension/batch/".length));
    const payload = readBatchPayload(batchId);
    if (!payload) {
      sendJson(res, 404, { ok: false, error: "Batch not found" }, origin);
      return true;
    }
    sendJson(res, 200, { ok: true, payload }, origin);
    return true;
  }

  if (req.method === "GET" && urlPath.startsWith("/extension/add-download/")) {
    const addId = decodeURIComponent(urlPath.slice("/extension/add-download/".length));
    const payload = readAddDownloadPayload(addId);
    if (!payload) {
      sendJson(res, 404, { ok: false, error: "Add download not found" }, origin);
      return true;
    }
    sendJson(res, 200, { ok: true, payload }, origin);
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
      focusAvarApp();
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
  setBatchPopupOpener,
  setAddDownloadPopupOpener,
  setAppFocusHandler,
  getExtensionBridgeState,
  createExtensionBridgeServer,
  DEFAULT_EXTENSION_GUI_URL: EXTENSION_GUI_URL,
  ELECTRON_EXTENSION_BRIDGE_URL: `http://${EXTENSION_BRIDGE_HOST}:${EXTENSION_BRIDGE_PORT}`,
  EXTENSION_BRIDGE_HOST,
  EXTENSION_BRIDGE_PORT,
  BRIDGE_VERSION,
  BUNDLED_EXTENSION_VERSION: "0.1.0",
};
