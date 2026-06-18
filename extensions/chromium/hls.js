/**
 * HLS master-playlist expansion for extension media lists.
 */

async function expandSingleHlsItem(item, referer, listVariants) {
  if (!item?.url) {
    return [];
  }

  try {
    const variants = await listVariants(item.url, referer);
    if (variants.length > 1) {
      const expanded = [];
      const localSeen = new Set([item.url]);
      for (const variant of variants) {
        if (!variant?.url || localSeen.has(variant.url)) {
          continue;
        }
        localSeen.add(variant.url);
        expanded.push({
          ...item,
          url: variant.url,
          kind: "hls",
          hlsLabel: variant.label || "HLS",
          hlsResolution: variant.resolution || null,
          masterUrl: item.url,
        });
      }
      return expanded;
    }

    if (variants.length === 1) {
      const variant = variants[0];
      return [
        {
          ...item,
          url: variant.url,
          kind: "hls",
          hlsLabel: variant.label || "HLS",
          hlsResolution: variant.resolution || null,
        },
      ];
    }
  } catch {
    // Keep the original playlist URL when variant discovery fails.
  }

  return [item];
}

async function expandHlsMediaItems(items, referer, listVariants) {
  const result = [];
  const seen = new Set();
  const hlsItems = [];

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

    if (!seen.has(item.url)) {
      hlsItems.push(item);
    }
  }

  const expandedGroups = await Promise.all(
    hlsItems.map((item) => expandSingleHlsItem(item, referer, listVariants)),
  );

  for (const group of expandedGroups) {
    for (const entry of group) {
      if (!entry?.url || seen.has(entry.url)) {
        continue;
      }
      seen.add(entry.url);
      result.push(entry);
    }
  }

  return result;
}

if (typeof globalThis !== "undefined") {
  globalThis.AvarHls = {
    expandHlsMediaItems,
  };
}
