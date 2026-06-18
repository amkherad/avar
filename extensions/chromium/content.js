chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type !== "avar-get-page-media") {
    return false;
  }

  if (typeof AvarMedia === "undefined") {
    sendResponse({ urls: [], items: [] });
    return true;
  }

  const items = AvarMedia.mergeMediaItems(
    AvarMedia.collectMediaItems(document),
    [...hookedMediaItems.values()],
  );
  sendResponse({
    urls: items.map((item) => item.url),
    items,
    pageTitle: document.title || "",
  });
  return true;
});

/** @type {Map<string, {url: string, kind: string}>} */
const hookedMediaItems = new Map();

function rememberHookedText(text) {
  if (!text || typeof AvarMedia === "undefined") {
    return;
  }
  const doc = document.implementation.createHTMLDocument("");
  const script = doc.createElement("script");
  script.textContent = text;
  doc.body.appendChild(script);
  for (const item of AvarMedia.collectMediaItems(doc)) {
    hookedMediaItems.set(item.url, item);
  }
}

function installResponseHook() {
  window.addEventListener("message", (event) => {
    if (event.source !== window || event.data?.type !== "avar-response-body") {
      return;
    }
    rememberHookedText(event.data.text);
  });

  const script = document.createElement("script");
  script.src = chrome.runtime.getURL("page-response-hook.js");
  script.addEventListener("error", () => {
    script.remove();
  });
  (document.documentElement || document.head || document.body).appendChild(script);
  script.addEventListener("load", () => {
    script.remove();
  });
}

installResponseHook();
