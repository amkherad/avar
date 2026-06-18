/**
 * Avar browser-extension messaging protocol (v1) — shared client helpers.
 */
/* global chrome, browser */

(() => {
  if (typeof globalThis !== "undefined" && globalThis.AvarExtensionProtocol) {
    return;
  }

  const PROTOCOL = "avar.extension";
  const PROTOCOL_VERSION = 1;

  const DEFAULT_ELECTRON_BRIDGE = "http://127.0.0.1:18766";
  const ELECTRON_BRIDGE_PORT = 18766;
  const LOCAL_BRIDGE_HOSTS = new Set(["127.0.0.1", "localhost", "[::1]"]);

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
    const url = normalizeBridgeUrl(baseUrl);
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
    const url = normalizeBridgeUrl(baseUrl);
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
   * Extensions must only talk to the Electron bridge — never the daemon or Vite dev server.
   * @param {string} [url]
   */
  function normalizeBridgeUrl(url) {
    if (!url) {
      return DEFAULT_ELECTRON_BRIDGE;
    }

    try {
      const parsed = new URL(url.replace(/\/+$/, ""));
      const port = Number(parsed.port || (parsed.protocol === "https:" ? 443 : 80));
      const host = parsed.hostname.toLowerCase();

      if (LOCAL_BRIDGE_HOSTS.has(host) && port === ELECTRON_BRIDGE_PORT) {
        return `${parsed.protocol}//${parsed.host}`.replace(/\/+$/, "");
      }
    } catch {
      // Fall through to the Electron default.
    }

    return DEFAULT_ELECTRON_BRIDGE;
  }

  /**
   * Resolve the Electron extension bridge URL.
   * @param {string} [storedUrl]
   */
  async function discoverBridgeUrl(storedUrl) {
    const normalized = normalizeBridgeUrl(storedUrl);
    try {
      await pingBridge(normalized);
      return normalized;
    } catch {
      return DEFAULT_ELECTRON_BRIDGE;
    }
  }

  if (typeof globalThis !== "undefined") {
    globalThis.AvarExtensionProtocol = {
      PROTOCOL,
      PROTOCOL_VERSION,
      DEFAULT_ELECTRON_BRIDGE,
      ELECTRON_BRIDGE_PORT,
      createMessage,
      sendMessage,
      pingBridge,
      normalizeBridgeUrl,
      discoverBridgeUrl,
    };
  }
})();
