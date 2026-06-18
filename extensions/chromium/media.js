/**
 * Shared media URL discovery for Avar browser extensions.
 * Loaded as a classic script in extension pages (no bundler).
 */

const MEDIA_EXT =
  /\.(mp4|webm|mkv|avi|mov|m4v|mp3|m4a|flac|wav|ogg|opus|aac|wma|png|jpe?g|gif|webp|svg|bmp|ico|zip|rar|7z|tar|gz|bz2|pdf|epub|mobi)(\?|#|$)/i;

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

function collectMediaUrls(doc) {
  const base = doc.baseURI || location.href;
  const urls = new Set();

  function add(raw) {
    const href = resolveUrl(raw, base);
    if (isFetchable(href)) {
      urls.add(href);
    }
  }

  doc.querySelectorAll("video, audio").forEach((el) => {
    if (el.src) {
      add(el.src);
    }
    el.querySelectorAll("source[src]").forEach((source) => add(source.src));
  });

  doc.querySelectorAll("a[href]").forEach((anchor) => {
    if (MEDIA_EXT.test(anchor.href)) {
      add(anchor.href);
    }
  });

  doc.querySelectorAll("img[src]").forEach((img) => {
    const src = resolveUrl(img.currentSrc || img.src, base);
    if (!isFetchable(src)) {
      return;
    }
    if (MEDIA_EXT.test(src) || img.naturalWidth >= 200 || img.naturalHeight >= 200) {
      urls.add(src);
    }
  });

  doc.querySelectorAll("link[rel='preload'][as='video'], link[rel='preload'][as='audio']").forEach(
    (link) => {
      if (link.href) {
        add(link.href);
      }
    },
  );

  return [...urls];
}

function guessFilename(url) {
  try {
    const pathname = new URL(url).pathname;
    const segment = pathname.split("/").filter(Boolean).pop();
    return segment ? decodeURIComponent(segment) : url;
  } catch {
    return url;
  }
}

// eslint-disable-next-line no-undef
if (typeof globalThis !== "undefined") {
  globalThis.AvarMedia = { collectMediaUrls, guessFilename, MEDIA_EXT };
}
