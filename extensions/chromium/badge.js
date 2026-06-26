/**
 * Toolbar badge showing how many capturable media URLs are on the active tab.
 */

function createBadgeController(api, options) {
  const { collectFromTab, debounceMs = 400 } = options;
  let activeTabId = null;
  let refreshTimer = null;

  function getAction() {
    return api.action || api.browserAction;
  }

  function applyBadge(count) {
    const action = getAction();
    if (!action) {
      return;
    }
    if (count > 0) {
      const text = count > 99 ? "99+" : String(count);
      void action.setBadgeText({ text });
      if (typeof action.setBadgeBackgroundColor === "function") {
        void action.setBadgeBackgroundColor({ color: "#2563eb" });
      }
    } else {
      void action.setBadgeText({ text: "" });
    }
  }

  async function refreshBadgeForTab(tabId) {
    if (tabId == null || tabId < 0) {
      if (activeTabId == null) {
        applyBadge(0);
      }
      return;
    }

    try {
      const result = await collectFromTab(tabId);
      const items = result?.items ?? [];
      const count = items.filter((item) => AvarMedia.shouldListMediaItem(item)).length;
      if (tabId === activeTabId) {
        applyBadge(count);
      }
    } catch {
      if (tabId === activeTabId) {
        applyBadge(0);
      }
    }
  }

  function scheduleRefresh(tabId) {
    if (tabId == null || tabId < 0) {
      return;
    }
    if (refreshTimer) {
      clearTimeout(refreshTimer);
    }
    refreshTimer = setTimeout(() => {
      refreshTimer = null;
      void refreshBadgeForTab(tabId);
    }, debounceMs);
  }

  function install() {
    if (api.tabs?.onActivated) {
      api.tabs.onActivated.addListener(({ tabId }) => {
        activeTabId = tabId;
        scheduleRefresh(tabId);
      });
    }

    if (api.tabs?.onRemoved) {
      api.tabs.onRemoved.addListener((tabId) => {
        if (tabId === activeTabId) {
          activeTabId = null;
          applyBadge(0);
        }
      });
    }

    if (api.tabs?.onUpdated) {
      api.tabs.onUpdated.addListener((tabId, changeInfo) => {
        if (tabId === activeTabId && (changeInfo.status === "complete" || changeInfo.url)) {
          scheduleRefresh(tabId);
        }
      });
    }

    void api.tabs.query({ active: true, currentWindow: true }).then((tabs) => {
      activeTabId = tabs[0]?.id ?? null;
      if (activeTabId != null) {
        scheduleRefresh(activeTabId);
      }
    });
  }

  return { install, scheduleRefresh };
}

globalThis.AvarBadge = { createBadgeController };
