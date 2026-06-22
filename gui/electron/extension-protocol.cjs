/**
 * Avar browser-extension messaging protocol (v1).
 *
 * Transport: HTTP JSON on the dedicated extension bridge port.
 *   POST /v1          — envelope message
 *   GET  /v1/ping     — lightweight health check
 *
 * Legacy REST (still supported on the bridge port):
 *   GET  /extension/ping
 *   GET  /extension/status
 *   POST /extension/download
 *   GET  /extension/batch/:id
 *   GET  /extension/add-download/:id
 *   POST /extension/settings
 */

const PROTOCOL = "avar.extension";
const PROTOCOL_VERSION = 1;
const BRIDGE_VERSION = "0.1.0";
const EXTENSION_BRIDGE_PORT = Number(process.env.AVAR_EXTENSION_BRIDGE_PORT || 18766);
const EXTENSION_BRIDGE_HOST = "127.0.0.1";

/** @typedef {'ping' | 'status' | 'download.add' | 'download.add.open' | 'download.batch.open'} ExtensionMessageType */

/**
 * @param {ExtensionMessageType} type
 * @param {Record<string, unknown>} [payload]
 * @param {string} [id]
 */
function createMessage(type, payload, id) {
  return {
    protocol: PROTOCOL,
    version: PROTOCOL_VERSION,
    type,
    id: id ?? String(Date.now()),
    payload: payload ?? {},
  };
}

/**
 * @param {unknown} body
 * @returns {{ ok: true, message: ReturnType<typeof createMessage> } | { ok: false, error: string }}
 */
function parseEnvelope(body) {
  if (!body || typeof body !== "object") {
    return { ok: false, error: "Expected JSON object" };
  }
  const msg = /** @type {Record<string, unknown>} */ (body);
  if (msg.protocol !== PROTOCOL) {
    return { ok: false, error: `Unknown protocol: ${msg.protocol}` };
  }
  if (msg.version !== PROTOCOL_VERSION) {
    return { ok: false, error: `Unsupported protocol version: ${msg.version}` };
  }
  if (typeof msg.type !== "string" || !msg.type) {
    return { ok: false, error: "Missing message type" };
  }
  return {
    ok: true,
    message: {
      protocol: PROTOCOL,
      version: PROTOCOL_VERSION,
      type: msg.type,
      id: typeof msg.id === "string" ? msg.id : String(Date.now()),
      payload: msg.payload && typeof msg.payload === "object" ? msg.payload : {},
    },
  };
}

/**
 * @param {ExtensionMessageType} type
 * @param {string} id
 * @param {Record<string, unknown>} [payload]
 */
function createResponse(type, id, payload) {
  return {
    protocol: PROTOCOL,
    version: PROTOCOL_VERSION,
    type,
    id,
    ok: true,
    payload: payload ?? {},
  };
}

function createErrorResponse(type, id, error) {
  return {
    protocol: PROTOCOL,
    version: PROTOCOL_VERSION,
    type,
    id,
    ok: false,
    error,
  };
}

module.exports = {
  PROTOCOL,
  PROTOCOL_VERSION,
  BRIDGE_VERSION,
  EXTENSION_BRIDGE_HOST,
  EXTENSION_BRIDGE_PORT,
  createMessage,
  parseEnvelope,
  createResponse,
  createErrorResponse,
};
