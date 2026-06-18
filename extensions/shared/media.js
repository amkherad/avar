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
  /videoplayback|\/hls\/|\/dash\/|hls_playlist|\/master\.txt(\?|#|$)/i;

const PREVIEW_URL_RE = /getVideoPreview|getPreview|\/preview\//i;

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

function looksLikeSignedMediaUrl(url) {
  if (!url) {
    return false;
  }
  try {
    const parsed = new URL(url);
    if (parsed.searchParams.has("sig") && parsed.searchParams.has("expires")) {
      return true;
    }
    if (parsed.searchParams.has("type") && parsed.searchParams.has("sig")) {
      return true;
    }
  } catch {
    return false;
  }
  return false;
}

function classifyMediaFilename(filename) {
  if (!filename) {
    return null;
  }
  if (HLS_URL_RE.test(filename) || /\.m3u8$/i.test(filename)) {
    return "hls";
  }
  if (DASH_URL_RE.test(filename) || /\.mpd$/i.test(filename)) {
    return "dash";
  }
  if (VIDEO_EXT.test(filename) || AUDIO_EXT.test(filename) || MEDIA_EXT.test(filename)) {
    return "direct";
  }
  return null;
}

function classifyMediaUrl(url) {
  if (!isFetchable(url) || PREVIEW_URL_RE.test(url)) {
    return null;
  }

  if (HLS_URL_RE.test(url)) {
    return { url, kind: "hls" };
  }
  if (DASH_URL_RE.test(url)) {
    return { url, kind: "dash" };
  }
  if (looksLikeMediaUrl(url)) {
    return { url, kind: "direct" };
  }
  if (looksLikeSignedMediaUrl(url)) {
    return { url, kind: classifyStreamKind(url) };
  }

  try {
    const parsed = new URL(url);
    if (/mime=video/i.test(parsed.search)) {
      return { url, kind: "direct" };
    }
    if (/mime=audio/i.test(parsed.search)) {
      return { url, kind: "direct" };
    }
  } catch {
    // ignore
  }

  if (NETWORK_STREAM_RE.test(url) && !PREVIEW_URL_RE.test(url)) {
    if (looksLikeMediaUrl(url) || HLS_URL_RE.test(url) || DASH_URL_RE.test(url)) {
      return { url, kind: classifyStreamKind(url) };
    }
    if (/videoplayback|\/hls\/|hls_playlist/i.test(url)) {
      return { url, kind: classifyStreamKind(url) };
    }
  }

  return null;
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
    const leftName = itemDisplayFilename(left);
    const rightName = itemDisplayFilename(right);
    return leftName.localeCompare(rightName);
  });
}

function sortMediaItemsByMode(items, mode, sizeForItem) {
  const list = [...items];
  if (mode === "size-asc" || mode === "size-desc") {
    return list.sort((left, right) => {
      const leftSize = sizeForItem ? sizeForItem(left) : left.size;
      const rightSize = sizeForItem ? sizeForItem(right) : right.size;
      const leftKnown = typeof leftSize === "number" && leftSize >= 0;
      const rightKnown = typeof rightSize === "number" && rightSize >= 0;

      if (!leftKnown && !rightKnown) {
        return itemDisplayFilename(left).localeCompare(itemDisplayFilename(right));
      }
      if (!leftKnown) {
        return 1;
      }
      if (!rightKnown) {
        return -1;
      }

      const delta = leftSize - rightSize;
      if (delta !== 0) {
        return mode === "size-asc" ? delta : -delta;
      }
      return itemDisplayFilename(left).localeCompare(itemDisplayFilename(right));
    });
  }

  return sortMediaItems(list);
}

function trimQuotes(value) {
  const trimmed = value.trim();
  if (
    (trimmed.startsWith('"') && trimmed.endsWith('"')) ||
    (trimmed.startsWith("'") && trimmed.endsWith("'"))
  ) {
    return trimmed.slice(1, -1);
  }
  return trimmed;
}

function parseContentDispositionFilename(value) {
  if (!value) {
    return "";
  }

  const starMatch = /filename\*\s*=\s*([^;]+)/i.exec(value);
  if (starMatch) {
    let encoded = starMatch[1].trim();
    if (encoded.toLowerCase().startsWith("utf-8''")) {
      encoded = encoded.slice(7);
    }
    encoded = trimQuotes(encoded);
    try {
      return decodeURIComponent(encoded);
    } catch {
      return encoded;
    }
  }

  const match = /filename\s*=\s*([^;]+)/i.exec(value);
  if (match) {
    return trimQuotes(match[1].trim());
  }

  return "";
}

function parseContentLength(value) {
  if (!value) {
    return null;
  }
  const parsed = Number(value);
  if (!Number.isFinite(parsed) || parsed < 0) {
    return null;
  }
  return parsed;
}

function getContentLengthFromHeaders(headers) {
  return parseContentLength(getResponseHeader(headers, "content-length"));
}

function extractFilenameFromHeaders(headers) {
  const fromDisposition = parseContentDispositionFilename(
    getResponseHeader(headers, "content-disposition"),
  );
  if (fromDisposition) {
    return fromDisposition;
  }

  for (const name of [
    "x-filename",
    "x-file-name",
    "x-suggested-filename",
    "x-download-filename",
  ]) {
    const value = getResponseHeader(headers, name);
    if (value) {
      const trimmed = trimQuotes(value.trim());
      if (trimmed) {
        return trimmed;
      }
    }
  }

  const contentLocation = getResponseHeader(headers, "content-location");
  if (contentLocation) {
    const fromLocation = guessFilenameFromUrl(contentLocation);
    if (fromLocation && isInferableUrlFilename(fromLocation, contentLocation)) {
      return fromLocation;
    }
  }

  return "";
}

function withCapturedMetadata(item, responseHeaders) {
  if (!item) {
    return null;
  }
  const next = { ...item };
  const filename = extractFilenameFromHeaders(responseHeaders);
  if (filename) {
    next.filename = filename;
  }
  const size = getContentLengthFromHeaders(responseHeaders);
  if (size !== null) {
    next.size = size;
  }
  return next;
}

function classifyCapturedRequest(url, responseHeaders) {
  if (!isFetchable(url)) {
    return null;
  }

  const contentType = normalizeMimeType(getResponseHeader(responseHeaders, "content-type"));
  const dispositionFilename = extractFilenameFromHeaders(responseHeaders);
  const dispositionKind = classifyMediaFilename(dispositionFilename);

  if (HLS_URL_RE.test(url) || looksLikeHlsType(contentType) || dispositionKind === "hls") {
    return withCapturedMetadata({ url, kind: "hls" }, responseHeaders);
  }
  if (DASH_URL_RE.test(url) || looksLikeDashType(contentType) || dispositionKind === "dash") {
    return withCapturedMetadata({ url, kind: "dash" }, responseHeaders);
  }

  if (VIDEO_MIME.test(contentType)) {
    return withCapturedMetadata({ url, kind: "direct" }, responseHeaders);
  }

  if (AUDIO_MIME.test(contentType)) {
    return withCapturedMetadata({ url, kind: "direct" }, responseHeaders);
  }

  if (IMAGE_MIME.test(contentType) && looksLikeMediaUrl(url)) {
    return withCapturedMetadata({ url, kind: "direct" }, responseHeaders);
  }

  if (dispositionKind === "direct") {
    return withCapturedMetadata({ url, kind: "direct" }, responseHeaders);
  }

  const urlClassified = classifyMediaUrl(url);
  if (urlClassified) {
    return withCapturedMetadata(urlClassified, responseHeaders);
  }

  if (
    contentType === "application/octet-stream" ||
    contentType === "binary/octet-stream"
  ) {
    const size = getContentLengthFromHeaders(responseHeaders);
    if (size === null || size >= 50_000) {
      const classified = classifyMediaUrl(url);
      if (classified) {
        return withCapturedMetadata(classified, responseHeaders);
      }
    }
  }

  return null;
}

function sanitizePageTitle(title) {
  if (!title) {
    return "";
  }
  return title.replace(/[<>:"/\\|?*\u0000-\u001f]/g, "").trim();
}

function isInferableUrlFilename(name, url) {
  if (!name || name === url) {
    return false;
  }
  if (MEDIA_EXT.test(name)) {
    return true;
  }
  if (/\.[a-z0-9]{2,5}$/i.test(name) && name.length > 4) {
    return true;
  }
  if (/^(videoplayback|index|stream|media|file|download|data|asset|chunk|segment)$/i.test(name)) {
    return false;
  }
  if (/^[a-f0-9-]{20,}$/i.test(name)) {
    return false;
  }
  if (!name.includes(".") && name.length <= 8) {
    return false;
  }
  return true;
}

function guessFilenameFromUrl(url) {
  if (!url) {
    return "";
  }

  try {
    const parsed = new URL(url);
    const queryNames = [
      "filename",
      "file",
      "download",
      "name",
      "title",
      "attachment",
      "response-content-disposition",
    ];

    for (const key of queryNames) {
      const value = parsed.searchParams.get(key);
      if (!value) {
        continue;
      }
      if (key === "response-content-disposition") {
        const fromDisposition = parseContentDispositionFilename(value);
        if (fromDisposition) {
          return fromDisposition;
        }
        continue;
      }
      const trimmed = trimQuotes(value.trim());
      if (trimmed) {
        try {
          return decodeURIComponent(trimmed);
        } catch {
          return trimmed;
        }
      }
    }
  } catch {
    return guessFilename(url);
  }

  return guessFilename(url);
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

function itemDisplayFilename(item, pageTitle) {
  if (item?.filename) {
    return item.filename;
  }

  const url = item?.url || "";
  const fromUrl = guessFilenameFromUrl(url);
  if (fromUrl && isInferableUrlFilename(fromUrl, url)) {
    return fromUrl;
  }

  const title = sanitizePageTitle(pageTitle || item?.pageTitle || "");
  if (title) {
    return title;
  }

  return fromUrl || url;
}

const HLS_SEGMENT_RE = /\.(ts|m4s|cmfv|cmfa|aac|vtt|key)(\?|#|$)/i;

function shouldListMediaItem(item) {
  if (!item?.url) {
    return false;
  }
  if (item.kind === "hls" || item.kind === "dash") {
    return true;
  }
  if (item.kind !== "direct") {
    return false;
  }
  if (HLS_SEGMENT_RE.test(item.url)) {
    return false;
  }
  return true;
}

function matchesMediaFilter(item, filter) {
  if (!filter || filter === "all") {
    return true;
  }
  return classifyMediaCategory(item) === filter;
}

function filterMediaItems(items, filter) {
  if (!filter || filter === "all") {
    return items;
  }
  return items.filter((item) => matchesMediaFilter(item, filter));
}

function formatFileSize(bytes) {
  if (!Number.isFinite(bytes) || bytes < 0) {
    return "—";
  }

  const units = ["B", "KB", "MB", "GB", "TB"];
  let size = bytes;
  let unitIndex = 0;
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024;
    unitIndex += 1;
  }

  const digits = size >= 100 || unitIndex === 0 ? 0 : size >= 10 ? 1 : 2;
  return `${size.toFixed(digits)} ${units[unitIndex]}`;
}

function formatDisplayUrl(url, maxLen = 48) {
  if (!url || url.length <= maxLen) {
    return url;
  }
  const head = Math.ceil((maxLen - 1) * 0.55);
  const tail = maxLen - 1 - head;
  return `${url.slice(0, head)}…${url.slice(-tail)}`;
}

function addMediaItem(map, url, kind, filename, size) {
  const href = typeof url === "string" ? url : null;
  if (!isFetchable(href)) {
    return;
  }
  const resolvedKind = kind || classifyStreamKind(href);
  const existing = map.get(href);
  if (!existing) {
    const item = { url: href, kind: resolvedKind };
    if (filename) {
      item.filename = filename;
    }
    if (typeof size === "number" && size >= 0) {
      item.size = size;
    }
    map.set(href, item);
    return;
  }

  const next = { ...existing };
  if (existing.kind === "direct" && resolvedKind !== "direct") {
    next.kind = resolvedKind;
  }
  if (filename && !next.filename) {
    next.filename = filename;
  }
  if (typeof size === "number" && size >= 0 && next.size == null) {
    next.size = size;
  }
  map.set(href, next);
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
      addMediaItem(map, item.url, item.kind, item.filename, item.size);
    }
  }
  return [...map.values()];
}

function extractStreamUrlsFromText(text, base, map) {
  if (!text) {
    return;
  }
  const hlsRe = /https?:\/\/[^\s"'<>]+\.m3u8[^\s"'<>]*/gi;
  const dashRe = /https?:\/\/[^\s"'<>]+\.mpd[^\s"'<>]*/gi;
  const playlistRe =
    /https?:\/\/[^\s"'<>]+(?:hls_playlist|videoplayback|\/hls\/|\/dash\/)[^\s"'<>]*/gi;
  const jsonEscapedUrlRe = /"(https?:(?:\\\/\\\/|\/\/)[^"]+)"/gi;
  const jsonMediaKeyRe =
    /"(?:url|src|source|hls|dash|stream|video|file|cache|download|media)(?:\d*)"\s*:\s*"(https?:[^"\\]+(?:\\.[^"\\]*)*)"/gi;

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
  while ((match = jsonEscapedUrlRe.exec(text)) !== null) {
    const raw = match[1].replace(/\\\//g, "/");
    const href = resolveUrl(raw, base);
    const classified = classifyMediaUrl(href);
    if (classified) {
      addMediaItem(map, classified.url, classified.kind);
    }
  }
  while ((match = jsonMediaKeyRe.exec(text)) !== null) {
    const raw = match[1].replace(/\\\//g, "/");
    const href = resolveUrl(raw, base);
    const classified = classifyMediaUrl(href);
    if (classified) {
      addMediaItem(map, classified.url, classified.kind);
    }
  }
}

function collectPerformanceMediaItems() {
  const map = new Map();
  if (typeof performance === "undefined" || typeof performance.getEntriesByType !== "function") {
    return [];
  }

  for (const entry of performance.getEntriesByType("resource")) {
    const classified = classifyMediaUrl(entry.name);
    if (classified) {
      addMediaItem(map, classified.url, classified.kind);
    }
  }

  return [...map.values()];
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

  if (doc.documentElement) {
    extractStreamUrlsFromText(doc.documentElement.innerHTML, base, map);
  }

  for (const item of collectPerformanceMediaItems()) {
    addMediaItem(map, item.url, item.kind, item.filename, item.size);
  }

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
    collectPerformanceMediaItems,
    mergeMediaItems,
    sortMediaItems,
    sortMediaItemsByMode,
    classifyStreamKind,
    classifyMediaUrl,
    classifyMediaCategory,
    classifyCapturedRequest,
    parseContentDispositionFilename,
    extractFilenameFromHeaders,
    parseContentLength,
    getContentLengthFromHeaders,
    guessFilename,
    guessFilenameFromUrl,
    isInferableUrlFilename,
    sanitizePageTitle,
    itemDisplayFilename,
    filterMediaItems,
    matchesMediaFilter,
    shouldListMediaItem,
    formatFileSize,
    formatDisplayUrl,
    MEDIA_EXT,
  };
}
