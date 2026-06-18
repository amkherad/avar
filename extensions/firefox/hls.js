/**
 * HLS master-playlist expansion for extension media lists.
 */

async function expandHlsMediaItems(items, referer, listVariants) {
  const result = [];
  const seen = new Set();

  for (const item of items) {
    if (typeof AvarMedia !== "undefined" && !AvarMedia.shouldListMediaItem(item)) {
      continue;
    }

    if (item?.kind !== "hls") {
      if (item?.url && !seen.has(item.url)) {
        seen.add(item.url);
        result.push(item);
      }
      continue;
    }

    if (!item?.url || seen.has(item.url)) {
      continue;
    }

    try {
      const variants = await listVariants(item.url, referer);
      if (variants.length > 1) {
        seen.add(item.url);
        for (const variant of variants) {
          if (!variant?.url || seen.has(variant.url)) {
            continue;
          }
          seen.add(variant.url);
          result.push({
            ...item,
            url: variant.url,
            kind: "hls",
            hlsLabel: variant.label || "HLS",
            hlsResolution: variant.resolution || null,
            masterUrl: item.url,
          });
        }
        continue;
      }

      if (variants.length === 1) {
        const variant = variants[0];
        seen.add(variant.url);
        result.push({
          ...item,
          url: variant.url,
          kind: "hls",
          hlsLabel: variant.label || "HLS",
          hlsResolution: variant.resolution || null,
        });
        continue;
      }
    } catch {
      // Keep the original playlist URL when variant discovery fails.
    }

    seen.add(item.url);
    result.push(item);
  }

  return result;
}

if (typeof globalThis !== "undefined") {
  globalThis.AvarHls = {
    expandHlsMediaItems,
  };
}
