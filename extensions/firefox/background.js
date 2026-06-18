const EXTENSION_VERSION = "0.1.0";
const api = typeof browser !== "undefined" ? browser : chrome;

const networkCapture = globalThis.AvarNetworkCapture.createNetworkMediaCapture(api);
networkCapture.installListeners();

// Inline protocol helpers (Firefox MV2 cannot importScripts from parent dirs in all builds).
const PROTOCOL = "avar.extension";
const PROTOCOL_VERSION = 1;
const DEFAULT_ELECTRON_BRIDGE = "http://127.0.0.1:18766";
const DEFAULT_VITE_BRIDGE = "http://127.0.0.1:5173";
const KNOWN_BRIDGE_URLS = [DEFAULT_ELECTRON_BRIDGE, DEFAULT_VITE_BRIDGE];

function createMessage(type, payload) {
  return {
    protocol: PROTOCOL,
    version: PROTOCOL_VERSION,
    type,
    id: String(Date.now()),
    payload: payload ?? {},
  };
}

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

async function getConfig() {
  const stored = await api.storage.local.get(["bridgeUrl", "guiUrl", "daemonUrl", "authToken"]);
  const storedUrl = stored.bridgeUrl || stored.guiUrl || stored.daemonUrl;
  return {
    bridgeUrl: (storedUrl || DEFAULT_ELECTRON_BRIDGE).replace(/\/+$/, ""),
    authToken: stored.authToken || "",
  };
}

async function resolveBridgeUrl() {
  const { bridgeUrl } = await getConfig();
  const discovered = await discoverBridgeUrl(bridgeUrl);
  if (discovered !== bridgeUrl) {
    await api.storage.local.set({
      bridgeUrl: discovered,
      guiUrl: discovered,
      daemonUrl: discovered,
    });
  }
  return discovered;
}

async function pingBridgeEndpoint() {
  const bridgeUrl = await resolveBridgeUrl();
  try {
    await pingBridge(bridgeUrl);
    return { ok: true, bridgeUrl };
  } catch (error) {
    return { ok: false, bridgeUrl, error: String(error) };
  }
}

async function addDownload(payload) {
  const bridgeUrl = await resolveBridgeUrl();
  const body =
    typeof payload === "string"
      ? { url: payload }
      : {
          url: payload.url,
          streamKind: payload.streamKind,
          referer: payload.referer,
        };
  await sendMessage(bridgeUrl, "download.add", body);
}

async function collectFromTab(tabId) {
  try {
    const response = await api.tabs.sendMessage(tabId, { type: "avar-get-page-media" });
    const domItems = response?.items || [];
    const capturedItems = networkCapture.getForTab(tabId);
    const items = AvarMedia.mergeMediaItems(domItems, capturedItems);
    return {
      urls: items.map((item) => item.url),
      items,
      pageUrl: null,
    };
  } catch {
    const capturedItems = networkCapture.getForTab(tabId);
    const items = AvarMedia.mergeMediaItems(capturedItems);
    return {
      urls: items.map((item) => item.url),
      items,
      pageUrl: null,
    };
  }
}

async function listMediaFromActiveTab() {
  const tabs = await api.tabs.query({ active: true, currentWindow: true });
  const tab = tabs[0];
  if (!tab?.id) {
    throw new Error("No active tab.");
  }
  const result = await collectFromTab(tab.id);
  result.pageUrl = tab.url || null;
  return result;
}

api.runtime.onInstalled.addListener(() => {
  api.contextMenus.create({
    id: "avar-download-page-media",
    title: "Download all media with Avar",
    contexts: ["page"],
  });
  api.contextMenus.create({
    id: "avar-download-link",
    title: "Download with Avar",
    contexts: ["link"],
  });
});

api.contextMenus.onClicked.addListener(async (info, tab) => {
  if (!tab?.id) {
    return;
  }

  try {
    if (info.menuItemId === "avar-download-link" && info.linkUrl) {
      await addDownload(info.linkUrl);
      return;
    }

    if (info.menuItemId === "avar-download-page-media") {
      const { items, urls } = await collectFromTab(tab.id);
      const media = items.length > 0 ? items : urls.map((url) => ({ url, kind: "direct" }));
      for (const item of media) {
        await addDownload({ url: item.url, streamKind: item.kind, referer: tab.url });
      }
    }
  } catch (error) {
    console.error("Avar extension:", error);
  }
});

api.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message?.type === "avar-add-download" && message.url) {
    const referer = message.referer || sender.tab?.url || null;
    addDownload({
      url: message.url,
      streamKind: message.streamKind,
      referer,
    })
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-list-media") {
    listMediaFromActiveTab()
      .then((result) =>
        sendResponse({
          ok: true,
          urls: result.urls,
          items: result.items,
        }),
      )
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-ping-bridge") {
    pingBridgeEndpoint()
      .then((result) => sendResponse(result))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-set-config") {
    const bridgeUrl = (message.bridgeUrl || message.guiUrl || message.daemonUrl || DEFAULT_ELECTRON_BRIDGE).replace(
      /\/+$/,
      "",
    );
    api.storage.local
      .set({
        bridgeUrl,
        guiUrl: bridgeUrl,
        daemonUrl: bridgeUrl,
        authToken: message.authToken || "",
      })
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  return false;
});

void resolveBridgeUrl().then(async (bridgeUrl) => {
  try {
    await sendMessage(bridgeUrl, "ping", { extensionVersion: EXTENSION_VERSION });
  } catch {
    // Bridge may not be running yet.
  }
});
