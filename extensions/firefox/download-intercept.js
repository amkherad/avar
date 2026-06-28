/**
 * Intercept native browser downloads and forward them to Avar.
 */

function createDownloadIntercept(api, handlers) {
  const { openSingleAdd, requireReachableBridge } = handlers;

  /** @type {Map<string, number>} */
  const recentUrls = new Map();

  function shouldInterceptUrl(url) {
    if (!url) {
      return false;
    }
    const lower = url.toLowerCase();
    if (
      lower.startsWith("blob:") ||
      lower.startsWith("data:") ||
      lower.startsWith("chrome-extension:") ||
      lower.startsWith("moz-extension:")
    ) {
      return false;
    }
    try {
      const parsed = new URL(url);
      return parsed.protocol === "http:" || parsed.protocol === "https:" || parsed.protocol === "ftp:";
    } catch {
      return false;
    }
  }

  function isDuplicate(url) {
    const now = Date.now();
    const last = recentUrls.get(url);
    if (last != null && now - last < 2000) {
      return true;
    }
    recentUrls.set(url, now);
    if (recentUrls.size > 100) {
      for (const [key, seenAt] of recentUrls) {
        if (now - seenAt > 5000) {
          recentUrls.delete(key);
        }
      }
    }
    return false;
  }

  async function resolveInterceptPageUrl(downloadItem) {
    const tabId = downloadItem.tabId;
    if (typeof tabId === "number" && tabId >= 0) {
      try {
        const tab = await api.tabs.get(tabId);
        if (tab?.url && /^https?:\/\//i.test(tab.url)) {
          return tab.url;
        }
      } catch {
        // Tab may already be closed.
      }
    }

    const fallback = downloadItem.referringPage || downloadItem.referrer || "";
    return fallback.trim() || undefined;
  }

  async function handleDownloadCreated(downloadItem) {
    const stored = await api.storage.local.get([
      "grabAllDownloads",
      "blockBrowserDownloads",
      "defaultQueueId",
    ]);
    const grabAllDownloads = stored.grabAllDownloads !== false;
    const blockBrowserDownloads = stored.blockBrowserDownloads !== false;
    if (!grabAllDownloads) {
      return;
    }

    const url = downloadItem.finalUrl || downloadItem.url;
    if (!shouldInterceptUrl(url)) {
      return;
    }

    const duplicate = isDuplicate(url);
    if (duplicate) {
      if (blockBrowserDownloads) {
        try {
          await api.downloads.cancel(downloadItem.id);
        } catch {
          // Download may already be complete.
        }
      }
      return;
    }

    if (blockBrowserDownloads) {
      try {
        await api.downloads.cancel(downloadItem.id);
      } catch (error) {
        console.error("Avar extension: failed to cancel browser download", error);
      }
    }

    if (!(await requireReachableBridge())) {
      return;
    }

    const pageUrl = await resolveInterceptPageUrl(downloadItem);
    const filename = downloadItem.filename || downloadItem.suggestedFilename || "";
    const classified =
      typeof AvarMedia !== "undefined" ? AvarMedia.classifyMediaUrl(url) : null;
    const streamKind = classified?.kind || "direct";
    const defaultQueueId = stored.defaultQueueId || null;

    try {
      await openSingleAdd({
        url,
        streamKind,
        filename,
        referer: pageUrl,
        pageUrl,
        pageTitle: downloadItem.title || "",
        defaultQueueId,
      });
    } catch (error) {
      console.error("Avar extension: failed to forward download", error);
    }
  }

  function installListeners() {
    if (!api.downloads?.onCreated) {
      return;
    }
    api.downloads.onCreated.addListener((item) => {
      void handleDownloadCreated(item);
    });
  }

  return { installListeners };
}

globalThis.AvarDownloadIntercept = { createDownloadIntercept };
