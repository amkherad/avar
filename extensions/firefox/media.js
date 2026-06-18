/**
 * Shared media URL discovery for Avar browser extensions.
 * Loaded as a classic script in extension pages (no bundler).
 */

const MEDIA_EXT =
  /\.(mp4|webm|mkv|avi|mov|m4v|mpg|mpeg|mpe|wmv|asf|flv|f4v|3gp|3g2|vob|rm|rmvb|divx|ts|m2ts|mts|m3u8|mpd|ismv|isma|mp3|m4a|flac|wav|ogg|opus|aac|wma|weba|png|jpe?g|gif|webp|svg|bmp|ico|tiff?|zip|rar|7z|tar|gz|bz2|xz|pdf|epub|mobi|apk|deb|rpm|dmg|exe|msi|iso)(\?|#|$)/i;

const MEDIA_MIME = /^(video|audio)\//i;

const HLS_MIME = /^(application\/vnd\.apple\.mpegurl|application\/x-mpegurl|audio\/mpegurl)/i;

const DASH_MIME = /^application\/dash\+xml/i;

const HLS_URL_RE = /\.m3u8(\?|#|$)|\/[^/?#]*\.m3u8/i;

const DASH_URL_RE = /\.mpd(\?|#|$)|\/[^/?#]*\.mpd/i;

const PAGE_URL_RE = /https?:\/\/[^\s"'<>]+/gi;

function resolveUrl(raw, base) {
  try {
    return new URL(raw, base).href;
  } catch {
    return null;
  }
}

function isFetchable(url) {
  return (
    url &&
    !url.startsWith("blob:") &&
    !url.startsWith("data:") &&
    !url.startsWith("javascript:")
  );
}

function looksLikeMediaUrl(url) {
  return MEDIA_EXT.test(url);
}

function looksLikeMediaType(type) {
  return Boolean(type && MEDIA_MIME.test(type));
}

function looksLikeHlsType(type) {
  return Boolean(type && HLS_MIME.test(type));
}

function looksLikeDashType(type) {
  return Boolean(type && DASH_MIME.test(type));
}

function classifyStreamKind(url) {
  if (!url) {
    return "direct";
  }
  if (HLS_URL_RE.test(url)) {
    return "hls";
  }
  if (DASH_URL_RE.test(url)) {
    return "dash";
  }
  return "direct";
}

function guessFilename(url) {
  try {
    const parsed = new URL(url);
    const pathname = parsed.pathname;
    const segment = pathname.split("/").filter(Boolean).pop();
    if (!segment) {
      return url;
    }
    const decoded = decodeURIComponent(segment);
    const withoutQuery = decoded.split("?")[0];
    return withoutQuery || decoded;
  } catch {
    return url;
  }
}

function formatDisplayUrl(url, maxLen = 48) {
  if (!url || url.length <= maxLen) {
    return url;
  }
  const head = Math.ceil((maxLen - 1) * 0.55);
  const tail = maxLen - 1 - head;
  return `${url.slice(0, head)}…${url.slice(-tail)}`;
}

function addMediaItem(map, url, kind) {
  const href = typeof url === "string" ? url : null;
  if (!isFetchable(href)) {
    return;
  }
  const resolvedKind = kind || classifyStreamKind(href);
  const existing = map.get(href);
  if (!existing || (existing.kind === "direct" && resolvedKind !== "direct")) {
    map.set(href, { url: href, kind: resolvedKind });
  }
}

function extractStreamUrlsFromText(text, base, map) {
  if (!text) {
    return;
  }
  const hlsRe = /https?:\/\/[^\s"'<>]+\.m3u8[^\s"'<>]*/gi;
  const dashRe = /https?:\/\/[^\s"'<>]+\.mpd[^\s"'<>]*/gi;
  let match;
  while ((match = hlsRe.exec(text)) !== null) {
    addMediaItem(map, resolveUrl(match[0], base), "hls");
  }
  while ((match = dashRe.exec(text)) !== null) {
    addMediaItem(map, resolveUrl(match[0], base), "dash");
  }
}

function collectMediaItems(doc) {
  const base = doc.baseURI || location.href;
  const map = new Map();

  function add(raw, type) {
    const href = resolveUrl(raw, base);
    if (!isFetchable(href)) {
      return;
    }
    let kind = classifyStreamKind(href);
    if (looksLikeHlsType(type)) {
      kind = "hls";
    } else if (looksLikeDashType(type)) {
      kind = "dash";
    } else if (!looksLikeMediaUrl(href) && !looksLikeMediaType(type)) {
      return;
    }
    addMediaItem(map, href, kind);
  }

  doc.querySelectorAll("video, audio").forEach((el) => {
    if (el.src) {
      add(el.src, el.type);
    }
    el.querySelectorAll("source[src]").forEach((source) => {
      add(source.src, source.type);
    });
  });

  doc.querySelectorAll("a[href]").forEach((anchor) => {
    if (looksLikeMediaUrl(anchor.href)) {
      add(anchor.href, null);
    }
  });

  doc.querySelectorAll("img[src]").forEach((img) => {
    const src = resolveUrl(img.currentSrc || img.src, base);
    if (!isFetchable(src)) {
      return;
    }
    if (looksLikeMediaUrl(src) || img.naturalWidth >= 200 || img.naturalHeight >= 200) {
      addMediaItem(map, src, classifyStreamKind(src));
    }
  });

  doc.querySelectorAll("link[rel='preload'][as='video'], link[rel='preload'][as='audio']").forEach(
    (link) => {
      if (link.href) {
        add(link.href, link.type);
      }
    },
  );

  doc.querySelectorAll("link[href]").forEach((link) => {
    if (looksLikeMediaUrl(link.href)) {
      add(link.href, link.type);
    }
  });

  doc.querySelectorAll("script:not([src])").forEach((script) => {
    extractStreamUrlsFromText(script.textContent, base, map);
  });

  doc.querySelectorAll("[data-src], [data-url], [data-hls], [data-stream]").forEach((el) => {
    for (const attr of ["data-src", "data-url", "data-hls", "data-stream"]) {
      const value = el.getAttribute(attr);
      if (value) {
        add(value, null);
      }
    }
  });

  return [...map.values()];
}

function collectMediaUrls(doc) {
  return collectMediaItems(doc).map((item) => item.url);
}

// eslint-disable-next-line no-undef
if (typeof globalThis !== "undefined") {
  globalThis.AvarMedia = {
    collectMediaUrls,
    collectMediaItems,
    guessFilename,
    formatDisplayUrl,
    classifyStreamKind,
    MEDIA_EXT,
  };
}
