/**
 * Page-world hook: observe dynamic media element sources and direct media requests.
 */
(function () {
  if (window.__avarMediaHookInstalled) {
    return;
  }
  window.__avarMediaHookInstalled = true;

  const MEDIA_URL_HINT_RE =
    /\.(m3u8|mpd|mp4|webm|mkv|mov)(\?|#|$)|videoplayback|mime=video|mime=audio|hls_playlist|\/hls\/|\/dash\/|[?&]sig=[^&]+/i;

  function looksCapturableUrl(url) {
    if (!url || typeof url !== "string") {
      return false;
    }
    if (url.startsWith("blob:") || url.startsWith("data:") || url.startsWith("javascript:")) {
      return false;
    }
    return MEDIA_URL_HINT_RE.test(url);
  }

  function postUrls(urls) {
    const unique = [...new Set(urls.filter(looksCapturableUrl))];
    if (unique.length > 0) {
      window.postMessage({ type: "avar-media-urls", urls: unique }, "*");
    }
  }

  function collectMediaElementUrls() {
    const urls = [];
    document.querySelectorAll("video, audio").forEach((el) => {
      if (el.currentSrc) {
        urls.push(el.currentSrc);
      }
      if (el.src) {
        urls.push(el.src);
      }
      el.querySelectorAll("source[src]").forEach((source) => {
        const src = source.getAttribute("src") || source.src;
        if (src) {
          urls.push(src);
        }
      });
    });
    return urls;
  }

  function reportMediaElements() {
    postUrls(collectMediaElementUrls());
  }

  function hookSrcProperty(proto) {
    const descriptor = Object.getOwnPropertyDescriptor(proto, "src");
    if (!descriptor?.set || !descriptor?.get) {
      return;
    }
    Object.defineProperty(proto, "src", {
      configurable: true,
      enumerable: descriptor.enumerable,
      get() {
        return descriptor.get.call(this);
      },
      set(value) {
        if (looksCapturableUrl(value)) {
          postUrls([value]);
        }
        return descriptor.set.call(this, value);
      },
    });
  }

  function hookSetAttribute(proto) {
    const original = proto.setAttribute;
    proto.setAttribute = function avarMediaSetAttribute(name, value) {
      if (name === "src" && looksCapturableUrl(value)) {
        postUrls([value]);
      }
      return original.call(this, name, value);
    };
  }

  hookSrcProperty(HTMLMediaElement.prototype);
  if (typeof HTMLSourceElement !== "undefined") {
    hookSrcProperty(HTMLSourceElement.prototype);
    hookSetAttribute(HTMLSourceElement.prototype);
  }
  hookSetAttribute(HTMLMediaElement.prototype);

  const observer = new MutationObserver(() => {
    reportMediaElements();
  });

  function startObserver() {
    const root = document.documentElement || document.body;
    if (!root) {
      return;
    }
    observer.observe(root, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: ["src"],
    });
    reportMediaElements();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", startObserver, { once: true });
  } else {
    startObserver();
  }

  const originalOpen = XMLHttpRequest.prototype.open;
  XMLHttpRequest.prototype.open = function avarMediaXhrOpen(method, url, ...rest) {
    if (looksCapturableUrl(url)) {
      postUrls([url]);
    }
    return originalOpen.call(this, method, url, ...rest);
  };
})();
