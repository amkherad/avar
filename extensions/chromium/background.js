importScripts("media.js", "capture.js", "protocol.js", "hls.js", "context-menu.js");

const EXTENSION_VERSION = "0.1.0";
const { IDS: MENU_IDS } = globalThis.AvarContextMenu;
const { discoverBridgeUrl, sendMessage, pingBridge, normalizeBridgeUrl, DEFAULT_ELECTRON_BRIDGE, BRIDGE_UNREACHABLE } =
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
  try {
    const discovered = await discoverBridgeUrl(bridgeUrl);
    const normalized = normalizeBridgeUrl(discovered);
    if (normalized !== bridgeUrl) {
      await chrome.storage.local.set({ bridgeUrl: normalized, guiUrl: normalized, daemonUrl: normalized });
    }
    return normalized;
  } catch {
    return normalizeBridgeUrl(bridgeUrl);
  }
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

async function openSingleAdd(payload) {
  const bridgeUrl = await resolveBridgeUrl();
  return sendMessage(bridgeUrl, "download.add.open", {
    url: payload.url,
    streamKind: payload.streamKind,
    filename: payload.filename,
    referer: payload.referer,
    pageUrl: payload.pageUrl,
    pageTitle: payload.pageTitle,
    defaultQueueId: payload.defaultQueueId,
    title: payload.title || "Add download",
  });
}

async function openDownloads(payload) {
  const items = Array.isArray(payload.items) ? payload.items : [];
  if (items.length === 0) {
    throw new Error("No items");
  }
  if (items.length === 1) {
    const item = items[0];
    return openSingleAdd({
      url: item.url,
      streamKind: item.streamKind,
      filename: item.filename,
      referer: item.referer || item.originalUrl || payload.pageUrl,
      pageUrl: payload.pageUrl,
      pageTitle: payload.pageTitle,
      defaultQueueId: payload.defaultQueueId,
    });
  }
  return openBatchAdd({
    items,
    pageUrl: payload.pageUrl,
    pageTitle: payload.pageTitle,
    defaultQueueId: payload.defaultQueueId,
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

async function collectFromTab(tabId, { forContextMenu = false } = {}) {
  try {
    const response = await chrome.tabs.sendMessage(tabId, {
      type: "avar-get-page-media",
      forContextMenu,
    });
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
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id) {
    throw new Error("No active tab.");
  }
  const result = await collectFromTab(tab.id);
  result.pageUrl = tab.url || null;
  result.pageTitle = result.pageTitle || tab.title || "";
  return result;
}

let lastSelectionCount = 0;

async function refreshContextMenus(hasSelectedLinks) {
  await globalThis.AvarContextMenu.setHasSelectedLinks(chrome.contextMenus, hasSelectedLinks);
}

function isBridgeUnreachableError(error) {
  const message = String(error?.message || error);
  return message.includes(BRIDGE_UNREACHABLE) || message.includes("Failed to fetch");
}

async function requireReachableBridge() {
  const result = await pingBridgeEndpoint();
  if (!result.ok) {
    return false;
  }
  return true;
}

async function openBatchAllMedia(tab) {
  if (!(await requireReachableBridge())) {
    return;
  }
  const { items, urls, pageTitle } = await collectFromTab(tab.id);
  const media = items.length > 0 ? items : urls.map((url) => ({ url, kind: "direct" }));
  const listed = media.filter((item) => AvarMedia.shouldListMediaItem(item));
  if (listed.length === 0) {
    return;
  }
  const stored = await chrome.storage.local.get(["defaultQueueId"]);
  await openBatchAdd({
    items: listed.map((item) => buildBatchItemFromMedia(item, pageTitle, tab.url)),
    pageUrl: tab.url,
    pageTitle: pageTitle || tab.title || "",
    defaultQueueId: stored.defaultQueueId || null,
  });
}

async function openBatchSelectedMedia(tab) {
  if (!(await requireReachableBridge())) {
    return;
  }
  const { selectedItems, pageTitle } = await collectFromTab(tab.id, { forContextMenu: true });
  const listed = selectedItems.filter((item) => item?.url);
  if (listed.length === 0) {
    return;
  }
  const stored = await chrome.storage.local.get(["defaultQueueId"]);
  await openDownloads({
    items: listed.map((item) => buildBatchItemFromMedia(item, pageTitle, tab.url)),
    pageUrl: tab.url,
    pageTitle: pageTitle || tab.title || "",
    defaultQueueId: stored.defaultQueueId || null,
  });
}

async function openLinkDownloadDialog(linkUrl, tab) {
  if (!(await requireReachableBridge())) {
    return;
  }
  const stored = await chrome.storage.local.get(["defaultQueueId"]);
  const classified = AvarMedia.classifyMediaUrl(linkUrl);
  const item = classified || { url: linkUrl, kind: "direct" };
  const pageTitle = tab.title || "";
  await openSingleAdd({
    url: linkUrl,
    streamKind: item.kind,
    filename: AvarMedia.itemDisplayFilename(item, pageTitle),
    referer: tab.url,
    pageUrl: tab.url,
    pageTitle,
    defaultQueueId: stored.defaultQueueId || null,
  });
}

chrome.runtime.onInstalled.addListener(() => {
  void globalThis.AvarContextMenu.install(chrome.contextMenus);
});

if (chrome.contextMenus.onShown) {
  chrome.contextMenus.onShown.addListener(async (_info, tab) => {
    if (!tab?.id) {
      return;
    }
    try {
      const { selectedItems } = await collectFromTab(tab.id, { forContextMenu: true });
      const count = selectedItems.length;
      lastSelectionCount = count;
      await refreshContextMenus(count > 0);
      if (typeof chrome.contextMenus.refresh === "function") {
        chrome.contextMenus.refresh();
      }
    } catch {
      await refreshContextMenus(false);
    }
  });
}

chrome.contextMenus.onClicked.addListener(async (info, tab) => {
  if (!tab?.id) {
    return;
  }

  try {
    if (info.menuItemId === MENU_IDS.DOWNLOAD_LINK && info.linkUrl) {
      await openLinkDownloadDialog(info.linkUrl, tab);
      return;
    }

    if (globalThis.AvarContextMenu.isDownloadAllId(info.menuItemId)) {
      await openBatchAllMedia(tab);
      return;
    }

    if (info.menuItemId === MENU_IDS.DOWNLOAD_SELECTED) {
      await openBatchSelectedMedia(tab);
      return;
    }
  } catch (error) {
    if (!isBridgeUnreachableError(error)) {
      console.error("Avar extension:", error);
    }
  }
});

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message?.type === "avar-selection-changed") {
    const count = typeof message.count === "number" ? message.count : 0;
    lastSelectionCount = count;
    refreshContextMenus(count > 0)
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-open-add-download" && message.item?.url) {
    openSingleAdd({
      url: message.item.url,
      streamKind: message.item.streamKind,
      filename: message.item.filename,
      referer: message.item.referer || message.item.originalUrl || message.pageUrl,
      pageUrl: message.pageUrl,
      pageTitle: message.pageTitle,
      defaultQueueId: message.defaultQueueId,
    })
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-open-downloads" && Array.isArray(message.items)) {
    openDownloads({
      items: message.items,
      pageUrl: message.pageUrl,
      pageTitle: message.pageTitle,
      defaultQueueId: message.defaultQueueId,
    })
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

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
    openSingleAdd({
      url: message.url,
      streamKind: message.streamKind,
      filename: message.filename,
      referer,
      pageUrl: referer,
      pageTitle: message.pageTitle || "",
      defaultQueueId: message.defaultQueueId,
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

void globalThis.AvarContextMenu.ensureMenus(chrome.contextMenus);
