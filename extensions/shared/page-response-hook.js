/**
 * Page-world hook: scan fetch/XHR text responses for media URLs (injected by content script).
 */
(function () {
  if (window.__avarResponseHookInstalled) {
    return;
  }
  window.__avarResponseHookInstalled = true;

  const MEDIA_HINT_RE =
    /\.m3u8|\.mpd|videoplayback|mime=video|mime=audio|hls_playlist|\/hls\/|\/dash\//i;

  function postBody(text) {
    if (typeof text === "string" && MEDIA_HINT_RE.test(text)) {
      window.postMessage({ type: "avar-response-body", text }, "*");
    }
  }

  const originalFetch = window.fetch;
  window.fetch = function avarResponseFetchWrapper(...args) {
    return originalFetch.apply(this, args).then((response) => {
      const contentType = response.headers?.get?.("content-type") || "";
      if (/json|text\/plain|javascript|text\/html/i.test(contentType)) {
        response
          .clone()
          .text()
          .then(postBody)
          .catch(() => {});
      }
      return response;
    });
  };

  const originalOpen = XMLHttpRequest.prototype.open;
  XMLHttpRequest.prototype.open = function avarResponseXhrOpen(method, url, ...rest) {
    this.addEventListener("load", function avarResponseXhrLoad() {
      postBody(this.responseText);
    });
    return originalOpen.call(this, method, url, ...rest);
  };
})();
