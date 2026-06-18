importScripts("media.js", "protocol.js");

const EXTENSION_VERSION = "0.1.0";
const { discoverBridgeUrl, sendMessage, pingBridge, DEFAULT_ELECTRON_BRIDGE } =
  globalThis.AvarExtensionProtocol;

async function getConfig() {
  const stored = await chrome.storage.local.get(["bridgeUrl", "guiUrl", "daemonUrl", "authToken"]);
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
    await chrome.storage.local.set({ bridgeUrl: discovered, guiUrl: discovered, daemonUrl: discovered });
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

async function addDownload(url) {
  const bridgeUrl = await resolveBridgeUrl();
  await sendMessage(bridgeUrl, "download.add", { url });
}

async function collectFromTab(tabId) {
  const [{ result }] = await chrome.scripting.executeScript({
    target: { tabId },
    func: () => (typeof AvarMedia !== "undefined" ? AvarMedia.collectMediaUrls(document) : []),
  });
  return result || [];
}

async function listMediaFromActiveTab() {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id) {
    throw new Error("No active tab.");
  }
  return collectFromTab(tab.id);
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
      await addDownload(info.linkUrl);
      return;
    }

    if (info.menuItemId === "avar-download-page-media") {
      const urls = await collectFromTab(tab.id);
      for (const url of urls) {
        await addDownload(url);
      }
    }
  } catch (error) {
    console.error("Avar extension:", error);
  }
});

chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type === "avar-add-download" && message.url) {
    addDownload(message.url)
      .then(() => sendResponse({ ok: true }))
      .catch((error) => sendResponse({ ok: false, error: String(error) }));
    return true;
  }

  if (message?.type === "avar-list-media") {
    listMediaFromActiveTab()
      .then((urls) => sendResponse({ ok: true, urls }))
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
    chrome.storage.local
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

// Announce extension version on startup.
void resolveBridgeUrl().then(async (bridgeUrl) => {
  try {
    await sendMessage(bridgeUrl, "ping", { extensionVersion: EXTENSION_VERSION });
  } catch {
    // Bridge may not be running yet.
  }
});
