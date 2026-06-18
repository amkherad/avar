chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type !== "avar-get-page-media") {
    return false;
  }

  if (typeof AvarMedia === "undefined") {
    sendResponse({ urls: [], items: [] });
    return true;
  }

  const items = AvarMedia.collectMediaItems(document);
  sendResponse({ urls: items.map((item) => item.url), items });
  return true;
});
