/**
 * Network media capture for extension background scripts.
 */

function createNetworkMediaCapture(api) {
  /** @type {Map<number, Map<string, {url: string, kind: string}>>} */
  const capturedByTab = new Map();

  function remember(tabId, url, kind) {
    if (tabId < 0 || !url || typeof AvarMedia === "undefined") {
      return;
    }
    let tabMap = capturedByTab.get(tabId);
    if (!tabMap) {
      tabMap = new Map();
      capturedByTab.set(tabId, tabMap);
    }
    const existing = tabMap.get(url);
    if (!existing || (existing.kind === "direct" && kind !== "direct")) {
      tabMap.set(url, { url, kind });
    }
  }

  function clearTab(tabId) {
    capturedByTab.delete(tabId);
  }

  function getForTab(tabId) {
    const tabMap = capturedByTab.get(tabId);
    if (!tabMap) {
      return [];
    }
    return [...tabMap.values()];
  }

  function onRequestCompleted(details) {
    if (!details?.url || details.tabId < 0 || typeof AvarMedia === "undefined") {
      return;
    }
    const captured = AvarMedia.classifyCapturedRequest(details.url, details.responseHeaders);
    if (captured) {
      remember(details.tabId, captured.url, captured.kind);
    }
  }

  function installListeners() {
    if (!api.webRequest?.onCompleted) {
      return;
    }

    api.webRequest.onCompleted.addListener(
      onRequestCompleted,
      { urls: ["<all_urls>"] },
      ["responseHeaders"],
    );

    api.tabs.onRemoved.addListener((tabId) => {
      clearTab(tabId);
    });

    api.tabs.onUpdated.addListener((tabId, changeInfo) => {
      if (changeInfo.url) {
        clearTab(tabId);
      }
    });
  }

  return {
    remember,
    clearTab,
    getForTab,
    installListeners,
  };
}

// eslint-disable-next-line no-undef
if (typeof globalThis !== "undefined") {
  globalThis.AvarNetworkCapture = {
    createNetworkMediaCapture,
  };
}
