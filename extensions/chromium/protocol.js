/**
 * Avar browser-extension messaging protocol (v1) — shared client helpers.
 */
/* global chrome, browser */

const PROTOCOL = "avar.extension";
const PROTOCOL_VERSION = 1;

const DEFAULT_ELECTRON_BRIDGE = "http://127.0.0.1:18766";
const DEFAULT_VITE_BRIDGE = "http://127.0.0.1:5173";

const KNOWN_BRIDGE_URLS = [DEFAULT_ELECTRON_BRIDGE, DEFAULT_VITE_BRIDGE];

/**
 * @param {string} type
 * @param {Record<string, unknown>} [payload]
 */
function createMessage(type, payload) {
  return {
    protocol: PROTOCOL,
    version: PROTOCOL_VERSION,
    type,
    id: String(Date.now()),
    payload: payload ?? {},
  };
}

/**
 * @param {string} baseUrl
 * @param {string} type
 * @param {Record<string, unknown>} [payload]
 */
async function sendMessage(baseUrl, type, payload) {
  const url = baseUrl.replace(/\/+$/, "");
  const response = await fetch(`${url}/v1`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(createMessage(type, payload)),
  });
  if (!response.ok) {
    throw new Error(`Bridge HTTP ${response.status}`);
  }
  const body = await response.json();
  if (!body?.ok) {
    throw new Error(body?.error || "Bridge request failed");
  }
  return body.payload ?? {};
}

/**
 * @param {string} baseUrl
 */
async function pingBridge(baseUrl) {
  const url = baseUrl.replace(/\/+$/, "");
  const response = await fetch(`${url}/v1/ping`);
  if (!response.ok) {
    throw new Error(`Bridge HTTP ${response.status}`);
  }
  const body = await response.json();
  if (!body?.ok) {
    throw new Error("Ping failed");
  }
  return body;
}

/**
 * Try known bridge URLs and return the first reachable one.
 * @param {string} [storedUrl]
 */
async function discoverBridgeUrl(storedUrl) {
  const candidates = [];
  if (storedUrl) {
    candidates.push(storedUrl.replace(/\/+$/, ""));
  }
  for (const url of KNOWN_BRIDGE_URLS) {
    if (!candidates.includes(url)) {
      candidates.push(url);
    }
  }

  for (const url of candidates) {
    try {
      await pingBridge(url);
      return url;
    } catch {
      // try next
    }
  }
  return storedUrl?.replace(/\/+$/, "") || DEFAULT_ELECTRON_BRIDGE;
}

// Export for service workers (importScripts) and module bundlers.
if (typeof globalThis !== "undefined") {
  globalThis.AvarExtensionProtocol = {
    PROTOCOL,
    PROTOCOL_VERSION,
    DEFAULT_ELECTRON_BRIDGE,
    DEFAULT_VITE_BRIDGE,
    KNOWN_BRIDGE_URLS,
    createMessage,
    sendMessage,
    pingBridge,
    discoverBridgeUrl,
  };
}
