const EXTENSION_VERSION = "0.1.0";
const api = typeof browser !== "undefined" ? browser : chrome;

const { discoverBridgeUrl, sendMessage, pingBridge, normalizeBridgeUrl, DEFAULT_ELECTRON_BRIDGE } =
  globalThis.AvarExtensionProtocol;

const networkCapture = globalThis.AvarNetworkCapture.createNetworkMediaCapture(api);
networkCapture.installListeners();

async function getConfig() {
  const stored = await api.storage.local.get(["bridgeUrl", "guiUrl", "daemonUrl", "authToken"]);
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
    await api.storage.local.set({ bridgeUrl: normalized, guiUrl: normalized, daemonUrl: normalized });
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

async function openBatchAdd(payload) {
  const bridgeUrl = await resolveBridgeUrl();
  return sendMessage(bridgeUrl, "download.batch.open", {
    items: payload.items,
    pageUrl: payload.pageUrl,
    pageTitle: payload.pageTitle,
    defaultQueueId: payload.defaultQueueId,
    title: payload.title || "Add downloads",
  });
}

function buildBatchItemFromMedia(item, pageTitle, pageUrl) {
  const linkName = AvarMedia.itemDisplayFilename(item, pageTitle);
  return {
    url: item.url,
    streamKind: item.kind,
    filename: linkName,
    linkName,
    fileType: AvarMedia.classifyMediaCategory(item),
    fileSize: typeof item.size === "number" && item.size >= 0 ? item.size : null,
    originalUrl: pageUrl,
    referer: pageUrl,
  };
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
    const response = await api.tabs.sendMessage(tabId, { type: "avar-get-page-media" });
    const domItems = response?.items || [];
    const capturedItems = networkCapture.getForTab(tabId);
    const items = AvarMedia.mergeMediaItems(domItems, capturedItems);
    return {
      urls: items.map((item) => item.url),
      items,
      selectedItems: response?.selectedItems || [],
      pageUrl: null,
      pageTitle: response?.pageTitle || null,
    };
  } catch {
    const capturedItems = networkCapture.getForTab(tabId);
    const items = AvarMedia.mergeMediaItems(capturedItems);
    return {
      urls: items.map((item) => item.url),
      items,
      selectedItems: [],
      pageUrl: null,
      pageTitle: null,
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
  result.pageTitle = result.pageTitle || tab.title || "";
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
      await addDownload({ url: info.linkUrl, referer: tab.url, autoStart: true });
      return;
    }

    if (info.menuItemId === "avar-download-page-media") {
      const { items, urls, pageTitle } = await collectFromTab(tab.id);
      const media = items.length > 0 ? items : urls.map((url) => ({ url, kind: "direct" }));
      const listed = media.filter((item) => AvarMedia.shouldListMediaItem(item));
      if (listed.length === 0) {
        return;
      }
      const stored = await api.storage.local.get(["defaultQueueId"]);
      await openBatchAdd({
        items: listed.map((item) => buildBatchItemFromMedia(item, pageTitle, tab.url)),
        pageUrl: tab.url,
        pageTitle: pageTitle || tab.title || "",
        defaultQueueId: stored.defaultQueueId || null,
      });
      return;
    }
  } catch (error) {
    console.error("Avar extension:", error);
  }
});

api.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message?.type === "avar-open-batch-add" && Array.isArray(message.items)) {
    openBatchAdd({
      items: message.items,
      pageUrl: message.pageUrl,
      pageTitle: message.pageTitle,
      defaultQueueId: message.defaultQueueId,
    })
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

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
          selectedItems: result.selectedItems,
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
    if (typeof message.showSelectionWidget === "boolean") {
      storageUpdate.showSelectionWidget = message.showSelectionWidget;
    }
    api.storage.local
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
