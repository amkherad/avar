/**
 * Shared media URL discovery for Avar browser extensions.
 * Loaded as a classic script in extension pages (no bundler).
 */

const MEDIA_EXT =
  /\.(mp4|webm|mkv|avi|mov|m4v|mpg|mpeg|mpe|wmv|asf|flv|f4v|3gp|3g2|vob|rm|rmvb|divx|ts|m2ts|mts|m3u8|mpd|ismv|isma|mp3|m4a|flac|wav|ogg|opus|aac|wma|weba|png|jpe?g|gif|webp|svg|bmp|ico|tiff?|zip|rar|7z|tar|gz|bz2|xz|pdf|epub|mobi|apk|deb|rpm|dmg|exe|msi|iso)(\?|#|$)/i;

const VIDEO_EXT = /\.(mp4|webm|mkv|avi|mov|m4v|mpg|mpeg|mpe|wmv|asf|flv|f4v|3gp|3g2|vob|rm|rmvb|divx|ts|m2ts|mts|ismv)(\?|#|$)/i;

const AUDIO_EXT = /\.(mp3|m4a|flac|wav|ogg|opus|aac|wma|weba|isma)(\?|#|$)/i;

const IMAGE_EXT = /\.(png|jpe?g|gif|webp|svg|bmp|ico|tiff?)(\?|#|$)/i;

const MEDIA_MIME = /^(video|audio)\//i;

const VIDEO_MIME = /^video\//i;

const AUDIO_MIME = /^audio\//i;

const IMAGE_MIME = /^image\//i;

const HLS_MIME = /^(application\/vnd\.apple\.mpegurl|application\/x-mpegurl|audio\/mpegurl)/i;

const DASH_MIME = /^application\/dash\+xml/i;

const HLS_URL_RE = /\.m3u8(\?|#|$)|\/[^/?#]*\.m3u8|\/hls_playlist\/|\/manifest\/hls/i;

const DASH_URL_RE = /\.mpd(\?|#|$)|\/[^/?#]*\.mpd/i;

const NETWORK_STREAM_RE =
  /videoplayback|googlevideo\.com|vkuservideo|vkuserlive|mycdn\.me|okcdn\.ru|\/hls\/|\/dash\/|\/master\.txt(\?|#|$)/i;

const MEDIA_CATEGORY_ORDER = {
  video: 0,
  audio: 1,
  image: 2,
  binary: 3,
};

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

function getResponseHeader(headers, name) {
  if (!headers || !name) {
    return "";
  }
  const target = name.toLowerCase();
  for (const header of headers) {
    if (header?.name?.toLowerCase() === target) {
      return header.value || "";
    }
  }
  return "";
}

function normalizeMimeType(value) {
  if (!value) {
    return "";
  }
  return value.split(";")[0].trim().toLowerCase();
}

function classifyMediaCategory(item) {
  const url = item?.url || "";
  const kind = item?.kind || classifyStreamKind(url);

  if (kind === "hls" || kind === "dash") {
    return "video";
  }

  if (VIDEO_EXT.test(url)) {
    return "video";
  }
  if (AUDIO_EXT.test(url)) {
    return "audio";
  }
  if (IMAGE_EXT.test(url)) {
    return "image";
  }

  try {
    const parsed = new URL(url);
    if (/mime=video/i.test(parsed.search)) {
      return "video";
    }
    if (/mime=audio/i.test(parsed.search)) {
      return "audio";
    }
  } catch {
    // ignore
  }

  if (NETWORK_STREAM_RE.test(url) && !IMAGE_EXT.test(url)) {
    return "video";
  }

  return "binary";
}

function mediaCategorySortKey(item) {
  return MEDIA_CATEGORY_ORDER[classifyMediaCategory(item)] ?? 99;
}

function sortMediaItems(items) {
  return [...items].sort((left, right) => {
    const categoryDelta = mediaCategorySortKey(left) - mediaCategorySortKey(right);
    if (categoryDelta !== 0) {
      return categoryDelta;
    }
    const leftName = guessFilename(left.url);
    const rightName = guessFilename(right.url);
    return leftName.localeCompare(rightName);
  });
}

function classifyCapturedRequest(url, responseHeaders) {
  if (!isFetchable(url)) {
    return null;
  }

  const contentType = normalizeMimeType(getResponseHeader(responseHeaders, "content-type"));

  if (HLS_URL_RE.test(url) || looksLikeHlsType(contentType)) {
    return { url, kind: "hls" };
  }
  if (DASH_URL_RE.test(url) || looksLikeDashType(contentType)) {
    return { url, kind: "dash" };
  }

  if (VIDEO_MIME.test(contentType)) {
    return { url, kind: "direct" };
  }

  if (AUDIO_MIME.test(contentType)) {
    return { url, kind: "direct" };
  }

  if (IMAGE_MIME.test(contentType) && looksLikeMediaUrl(url)) {
    return { url, kind: "direct" };
  }

  try {
    const parsed = new URL(url);
    if (/googlevideo\.com$/i.test(parsed.hostname) && /videoplayback/i.test(parsed.pathname)) {
      return { url, kind: "direct" };
    }
    if (/mime=video/i.test(parsed.search)) {
      return { url, kind: "direct" };
    }
    if (/mime=audio/i.test(parsed.search)) {
      return { url, kind: "direct" };
    }
  } catch {
    // ignore
  }

  if (NETWORK_STREAM_RE.test(url) && (looksLikeMediaUrl(url) || HLS_URL_RE.test(url) || DASH_URL_RE.test(url))) {
    return { url, kind: classifyStreamKind(url) };
  }

  if (NETWORK_STREAM_RE.test(url) && /videoplayback|\/hls\/|hls_playlist/i.test(url)) {
    return { url, kind: classifyStreamKind(url) };
  }

  return null;
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

function mergeMediaItems(...lists) {
  const map = new Map();
  for (const list of lists) {
    if (!Array.isArray(list)) {
      continue;
    }
    for (const item of list) {
      if (!item?.url) {
        continue;
      }
      addMediaItem(map, item.url, item.kind);
    }
  }
  return sortMediaItems([...map.values()]);
}

function extractStreamUrlsFromText(text, base, map) {
  if (!text) {
    return;
  }
  const hlsRe = /https?:\/\/[^\s"'<>]+\.m3u8[^\s"'<>]*/gi;
  const dashRe = /https?:\/\/[^\s"'<>]+\.mpd[^\s"'<>]*/gi;
  const playlistRe =
    /https?:\/\/[^\s"'<>]+(?:hls_playlist|videoplayback|\/hls\/|\/dash\/)[^\s"'<>]*/gi;
  let match;
  while ((match = hlsRe.exec(text)) !== null) {
    addMediaItem(map, resolveUrl(match[0], base), "hls");
  }
  while ((match = dashRe.exec(text)) !== null) {
    addMediaItem(map, resolveUrl(match[0], base), "dash");
  }
  while ((match = playlistRe.exec(text)) !== null) {
    const href = resolveUrl(match[0], base);
    addMediaItem(map, href, classifyStreamKind(href));
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

  return sortMediaItems([...map.values()]);
}

function collectMediaUrls(doc) {
  return collectMediaItems(doc).map((item) => item.url);
}

// eslint-disable-next-line no-undef
if (typeof globalThis !== "undefined") {
  globalThis.AvarMedia = {
    collectMediaUrls,
    collectMediaItems,
    mergeMediaItems,
    sortMediaItems,
    classifyStreamKind,
    classifyMediaCategory,
    classifyCapturedRequest,
    guessFilename,
    formatDisplayUrl,
    MEDIA_EXT,
  };
}
