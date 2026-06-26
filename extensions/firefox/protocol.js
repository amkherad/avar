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
  const AVAR_FOCUS_URL = "avar://focus";
  const LOCAL_BRIDGE_HOSTS = new Set(["127.0.0.1", "localhost", "[::1]"]);
  const BRIDGE_UNREACHABLE = "Cannot reach Avar bridge";
  const BRIDGE_FETCH_TIMEOUT_MS = 2000;

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
  async function fetchBridge(url, init, timeoutMs = BRIDGE_FETCH_TIMEOUT_MS) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeoutMs);
    try {
      return await fetch(url, { ...init, signal: controller.signal });
    } catch {
      throw new Error(BRIDGE_UNREACHABLE);
    } finally {
      clearTimeout(timer);
    }
  }

  async function sendMessage(baseUrl, type, payload) {
    const url = normalizeBridgeUrl(baseUrl);
    const response = await fetchBridge(`${url}/v1`, {
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
    const response = await fetchBridge(`${url}/v1/ping`);
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
      if (normalized !== DEFAULT_ELECTRON_BRIDGE) {
        try {
          await pingBridge(DEFAULT_ELECTRON_BRIDGE);
          return DEFAULT_ELECTRON_BRIDGE;
        } catch {
          throw new Error(BRIDGE_UNREACHABLE);
        }
      }
      throw new Error(BRIDGE_UNREACHABLE);
    }
  }

  /**
   * Open avar://focus to launch or bring the desktop app to the foreground.
   * @param {{ tabs?: { create: Function, remove?: Function } }} api
   */
  function wakeAvarApp(api) {
    if (!api?.tabs?.create) {
      return Promise.resolve(false);
    }

    const created = api.tabs.create({ url: AVAR_FOCUS_URL, active: false });
    const onTab = (tab) => {
      const tabId = tab?.id;
      if (tabId != null && api.tabs?.remove) {
        const removed = api.tabs.remove(tabId);
        if (removed?.catch) {
          removed.catch(() => {});
        }
      }
      return true;
    };

    if (created && typeof created.then === "function") {
      return created.then(onTab).catch(() => false);
    }

    return new Promise((resolve) => {
      api.tabs.create({ url: AVAR_FOCUS_URL, active: false }, (tab) => {
        resolve(onTab(tab));
      });
    });
  }

  function sleep(ms) {
    return new Promise((resolve) => {
      setTimeout(resolve, ms);
    });
  }

  /**
   * Wake Avar when needed and wait for the extension bridge to respond.
   * @param {{ tabs?: { create: Function, remove?: Function } }} api
   * @param {() => Promise<{ ok: boolean }>} pingFn
   * @param {{ maxAttempts?: number }} [options]
   */
  async function ensureBridgeReachable(api, pingFn, options = {}) {
    const maxAttempts = options.maxAttempts ?? 10;
    const first = await pingFn();
    if (first?.ok) {
      void wakeAvarApp(api);
      return first;
    }

    void wakeAvarApp(api);
    for (let attempt = 0; attempt < maxAttempts; attempt++) {
      await sleep(250 + attempt * 150);
      const result = await pingFn();
      if (result?.ok) {
        return result;
      }
    }
    return pingFn();
  }

  if (typeof globalThis !== "undefined") {
    globalThis.AvarExtensionProtocol = {
      PROTOCOL,
      PROTOCOL_VERSION,
      DEFAULT_ELECTRON_BRIDGE,
      ELECTRON_BRIDGE_PORT,
      AVAR_FOCUS_URL,
      BRIDGE_UNREACHABLE,
      createMessage,
      sendMessage,
      pingBridge,
      normalizeBridgeUrl,
      discoverBridgeUrl,
      wakeAvarApp,
      ensureBridgeReachable,
    };
  }
})();
