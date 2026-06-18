const api = typeof browser !== "undefined" ? browser : chrome;

api.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type !== "avar-get-page-media") {
    return false;
  }

  const urls = typeof AvarMedia !== "undefined" ? AvarMedia.collectMediaUrls(document) : [];
  sendResponse({ urls });
  return true;
});
