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

  async function resolveInterceptTab(tabId) {
    if (typeof tabId !== "number" || tabId < 0) {
      return null;
    }
    try {
      return await api.tabs.get(tabId);
    } catch {
      return null;
    }
  }

  async function resolveInterceptPageUrl(downloadItem) {
    const tab = await resolveInterceptTab(downloadItem.tabId);
    const tabUrl = tab?.url && /^https?:\/\//i.test(tab.url) ? tab.url : undefined;
    const fallback =
      typeof globalThis.AvarExtensionProtocol !== "undefined"
        ? globalThis.AvarExtensionProtocol.resolvePageReferer(
            downloadItem.referringPage,
            downloadItem.referrer,
          )
        : (downloadItem.referringPage || downloadItem.referrer || "").trim() || undefined;

    if (typeof globalThis.AvarExtensionProtocol !== "undefined") {
      return globalThis.AvarExtensionProtocol.resolvePageReferer(tabUrl, fallback);
    }
    return tabUrl || fallback || undefined;
  }

  async function resolveInterceptPageTitle(downloadItem) {
    const tab = await resolveInterceptTab(downloadItem.tabId);
    if (tab?.title) {
      return tab.title;
    }
    return downloadItem.title || "";
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
    const pageTitle = await resolveInterceptPageTitle(downloadItem);
    const rawFilename = downloadItem.filename || downloadItem.suggestedFilename || "";
    const classified =
      typeof AvarMedia !== "undefined" ? AvarMedia.classifyMediaUrl(url) : null;
    const streamKind = classified?.kind || "direct";
    const filename =
      typeof AvarMedia !== "undefined"
        ? AvarMedia.itemDisplayFilename(
            { url, filename: rawFilename || undefined, kind: streamKind },
            pageTitle,
          )
        : rawFilename;
    const defaultQueueId = stored.defaultQueueId || null;

    try {
      await openSingleAdd({
        url,
        streamKind,
        filename,
        referer: pageUrl,
        pageUrl,
        pageTitle,
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
