const api = typeof browser !== "undefined" ? browser : chrome;

/** @type {Map<string, object>} */
const hookedMediaItems = new Map();

function rememberClassifiedItems(items) {
  if (!Array.isArray(items) || typeof AvarMedia === "undefined") {
    return;
  }
  for (const item of items) {
    if (!item?.url) {
      continue;
    }
    const existing = hookedMediaItems.get(item.url);
    hookedMediaItems.set(item.url, existing ? AvarMedia.mergeMediaItems([existing, item])[0] : item);
  }
}

function rememberHookedText(text) {
  if (!text || typeof AvarMedia === "undefined") {
    return;
  }
  const doc = document.implementation.createHTMLDocument("");
  const script = doc.createElement("script");
  script.textContent = text;
  doc.body.appendChild(script);
  rememberClassifiedItems(AvarMedia.collectMediaItems(doc));
}

function rememberHookedUrls(urls) {
  if (!Array.isArray(urls) || typeof AvarMedia === "undefined") {
    return;
  }
  const items = [];
  for (const url of urls) {
    const classified = AvarMedia.classifyMediaUrl(url);
    if (classified) {
      items.push(classified);
    }
  }
  rememberClassifiedItems(items);
}

function injectPageScript(filename) {
  const script = document.createElement("script");
  script.src = api.runtime.getURL(filename);
  script.addEventListener("error", () => {
    script.remove();
  });
  (document.documentElement || document.head || document.body).appendChild(script);
  script.addEventListener("load", () => {
    script.remove();
  });
}

function installPageHooks() {
  window.addEventListener("message", (event) => {
    if (event.source !== window) {
      return;
    }
    if (event.data?.type === "avar-response-body") {
      rememberHookedText(event.data.text);
      return;
    }
    if (event.data?.type === "avar-media-urls") {
      rememberHookedUrls(event.data.urls);
    }
  });

  injectPageScript("page-response-hook.js");
  injectPageScript("media-hook.js");
}

function collectPageMedia() {
  return AvarMedia.mergeMediaItems(
    AvarMedia.collectMediaItems(document),
    AvarMedia.collectDomVideoItems(document),
    [...hookedMediaItems.values()],
  );
}

api.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type !== "avar-get-page-media") {
    return false;
  }

  if (typeof AvarMedia === "undefined") {
    sendResponse({ urls: [], items: [] });
    return true;
  }

  const items = collectPageMedia();
  sendResponse({
    urls: items.map((item) => item.url),
    items,
    pageTitle: document.title || "",
  });
  return true;
});

installPageHooks();
