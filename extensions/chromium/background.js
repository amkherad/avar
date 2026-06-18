importScripts("media.js", "capture.js", "protocol.js", "hls.js");

const EXTENSION_VERSION = "0.1.0";
const { discoverBridgeUrl, sendMessage, pingBridge, normalizeBridgeUrl, DEFAULT_ELECTRON_BRIDGE } =
  globalThis.AvarExtensionProtocol;

const networkCapture = globalThis.AvarNetworkCapture.createNetworkMediaCapture(chrome);
networkCapture.installListeners();

async function getConfig() {
  const stored = await chrome.storage.local.get(["bridgeUrl", "guiUrl", "daemonUrl", "authToken"]);
  const storedUrl = stored.bridgeUrl || stored.guiUrl || stored.daemonUrl;
  return {
    bridgeUrl: normalizeBridgeUrl(storedUrl || DEFAULT_ELECTRON_BRIDGE),
    authToken: stored.authToken || "",
  };
}

async function resolveBridgeUrl() {
  const { bridgeUrl } = await getConfig();
  const discovered = await discoverBridgeUrl(bridgeUrl);
  const normalized = normalizeBridgeUrl(discovered);
  if (normalized !== bridgeUrl) {
    await chrome.storage.local.set({ bridgeUrl: normalized, guiUrl: normalized, daemonUrl: normalized });
  }
  return normalized;
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
      ? { url: payload, startNow: true }
      : {
          url: payload.url,
          streamKind: payload.streamKind,
          referer: payload.referer,
          queue: payload.queue,
          startNow: payload.autoStart !== false,
        };
  await sendMessage(bridgeUrl, "download.add", body);
}

async function probeUrlSize(url, referer) {
  const bridgeUrl = await resolveBridgeUrl();
  return sendMessage(bridgeUrl, "url.probe", { url, referer });
}

async function listHlsVariants(url, referer) {
  const bridgeUrl = await resolveBridgeUrl();
  const payload = await sendMessage(bridgeUrl, "hls.list", { url, referer });
  return Array.isArray(payload.variants) ? payload.variants : [];
}

async function expandHlsItems(items, referer) {
  return AvarHls.expandHlsMediaItems(items, referer, listHlsVariants);
}

async function listQueues() {
  const bridgeUrl = await resolveBridgeUrl();
  const payload = await sendMessage(bridgeUrl, "queue.list", {});
  return Array.isArray(payload.queues) ? payload.queues : [];
}

async function controlQueue(action, queueId) {
  const bridgeUrl = await resolveBridgeUrl();
  await sendMessage(bridgeUrl, action, { id: queueId });
}

async function collectFromTab(tabId) {
  try {
    const response = await chrome.tabs.sendMessage(tabId, { type: "avar-get-page-media" });
    const domItems = response?.items || [];
    const capturedItems = networkCapture.getForTab(tabId);
    const items = AvarMedia.mergeMediaItems(domItems, capturedItems);
    return {
      urls: items.map((item) => item.url),
      items,
      pageUrl: null,
      pageTitle: response?.pageTitle || null,
    };
  } catch {
    const capturedItems = networkCapture.getForTab(tabId);
    const items = AvarMedia.mergeMediaItems(capturedItems);
    return {
      urls: items.map((item) => item.url),
      items,
      pageUrl: null,
      pageTitle: null,
    };
  }
}

async function listMediaFromActiveTab() {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id) {
    throw new Error("No active tab.");
  }
  const result = await collectFromTab(tab.id);
  result.pageUrl = tab.url || null;
  result.pageTitle = result.pageTitle || tab.title || "";
  return result;
}

chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.create({
    id: "avar-download-page-media",
    title: "Download all media with Avar",
    contexts: ["page", "frame"],
  });
  chrome.contextMenus.create({
    id: "avar-download-link",
    title: "Download with Avar",
    contexts: ["link"],
  });
});

chrome.contextMenus.onClicked.addListener(async (info, tab) => {
  if (!tab?.id) {
    return;
  }

  try {
    if (info.menuItemId === "avar-download-link" && info.linkUrl) {
      await addDownload({ url: info.linkUrl, referer: tab.url, autoStart: true });
      return;
    }

    if (info.menuItemId === "avar-download-page-media") {
      const { items, urls } = await collectFromTab(tab.id);
      const media = items.length > 0 ? items : urls.map((url) => ({ url, kind: "direct" }));
      for (const item of media) {
        await addDownload({ url: item.url, streamKind: item.kind, referer: tab.url, autoStart: true });
      }
    }
  } catch (error) {
    console.error("Avar extension:", error);
  }
});

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message?.type === "avar-add-download" && message.url) {
    const referer = message.referer || sender.tab?.url || null;
    addDownload({
      url: message.url,
      streamKind: message.streamKind,
      referer,
      queue: message.queue,
      autoStart: message.autoStart !== false,
    })
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-probe-size" && message.url) {
    probeUrlSize(message.url, message.referer)
      .then((payload) =>
        sendResponse({
          ok: true,
          size: payload.size ?? null,
          filename: payload.filename ?? null,
        }),
      )
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-expand-hls-items" && Array.isArray(message.items)) {
    expandHlsItems(message.items, message.referer)
      .then((items) => sendResponse({ ok: true, items }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-list-queues") {
    listQueues()
      .then((queues) => sendResponse({ ok: true, queues }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-queue-start" && message.queueId) {
    controlQueue("queue.start", message.queueId)
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-queue-stop" && message.queueId) {
    controlQueue("queue.stop", message.queueId)
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
          pageUrl: result.pageUrl,
          pageTitle: result.pageTitle,
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
    const bridgeUrl = normalizeBridgeUrl(
      message.bridgeUrl || message.guiUrl || message.daemonUrl || DEFAULT_ELECTRON_BRIDGE,
    );
    const storageUpdate = {
      bridgeUrl,
      guiUrl: bridgeUrl,
      daemonUrl: bridgeUrl,
      authToken: message.authToken || "",
    };
    if (typeof message.defaultQueueId === "string") {
      storageUpdate.defaultQueueId = message.defaultQueueId;
    }
    if (typeof message.defaultMediaFilter === "string") {
      storageUpdate.defaultMediaFilter = message.defaultMediaFilter;
    }
    chrome.storage.local
      .set(storageUpdate)
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
