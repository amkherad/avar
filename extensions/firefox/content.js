const api = typeof browser !== "undefined" ? browser : chrome;

const MIN_SELECTED_LINKS_FOR_WIDGET = 1;
const SELECTION_WIDGET_DEBOUNCE_MS = 100;
const SELECTED_LINKS_CACHE_MS = 60_000;
const CONTEXT_MENU_SNAPSHOT_MS = 30_000;

/** @type {Map<string, object>} */
const hookedMediaItems = new Map();

let showSelectionWidget = true;
let selectionWidgetOpacity = 100;
const MIN_SELECTION_WIDGET_OPACITY = 40;
const MAX_SELECTION_WIDGET_OPACITY = 100;
/** @type {HTMLElement | null} */
let selectionWidgetHost = null;
let selectionChangeTimer = null;
let widgetDismissed = false;
let lastWidgetLinkCount = 0;
let lastWidgetSelectionKey = "";
/** @type {{ left: number; top: number } | null} */
let widgetManualPosition = null;
/** @type {{ startX: number; startY: number; origLeft: number; origTop: number } | null} */
let widgetDragState = null;
/** @type {{ x: number; y: number }} */
let lastPointer = { x: 0, y: 0 };
/** @type {object[]} */
let cachedSelectedItems = [];
let cachedSelectedAt = 0;
/** @type {object[]} */
let contextMenuSnapshot = [];
let contextMenuSnapshotAt = 0;

let contentScriptRetired = false;

function isExtensionContextValid() {
  try {
    return Boolean(api.runtime?.id);
  } catch {
    return false;
  }
}

function ensureExtensionContext() {
  if (contentScriptRetired) {
    return false;
  }
  if (isExtensionContextValid()) {
    return true;
  }
  contentScriptRetired = true;
  if (selectionChangeTimer) {
    clearTimeout(selectionChangeTimer);
    selectionChangeTimer = null;
  }
  removeSelectionWidget();
  return false;
}

function sendRuntimeMessage(message) {
  if (!ensureExtensionContext()) {
    return Promise.resolve(undefined);
  }
  return api.runtime.sendMessage(message).catch(() => {});
}

const WIDGET_MARGIN_PX = 8;
const WIDGET_VIEWPORT_PADDING_PX = 8;

function rememberClassifiedItems(items) {
  if (!Array.isArray(items) || typeof AvarMedia === "undefined") {
    return;
  }
  for (const item of items) {
    if (!item?.url) {
      continue;
    }
    const existing = hookedMediaItems.get(item.url);
    hookedMediaItems.set(item.url, existing ? AvarMedia.mergeMediaItems([existing, item])[0] : item);
  }
}

function rememberHookedText(text) {
  if (!text || typeof AvarMedia === "undefined") {
    return;
  }
  const doc = document.implementation.createHTMLDocument("");
  const script = doc.createElement("script");
  script.textContent = text;
  doc.body.appendChild(script);
  rememberClassifiedItems(AvarMedia.collectMediaItems(doc));
}

function rememberHookedUrls(urls) {
  if (!Array.isArray(urls) || typeof AvarMedia === "undefined") {
    return;
  }
  const items = [];
  for (const url of urls) {
    const classified = AvarMedia.classifyMediaUrl(url);
    if (classified) {
      items.push(classified);
    }
  }
  rememberClassifiedItems(items);
}

function injectPageScript(filename) {
  if (!ensureExtensionContext()) {
    return;
  }
  const script = document.createElement("script");
  script.src = api.runtime.getURL(filename);
  script.addEventListener("error", () => {
    script.remove();
  });
  (document.documentElement || document.head || document.body).appendChild(script);
  script.addEventListener("load", () => {
    script.remove();
  });
}

function installPageHooks() {
  window.addEventListener("message", (event) => {
    if (event.source !== window) {
      return;
    }
    if (event.data?.type === "avar-response-body") {
      rememberHookedText(event.data.text);
      return;
    }
    if (event.data?.type === "avar-media-urls") {
      rememberHookedUrls(event.data.urls);
    }
  });

  injectPageScript("page-response-hook.js");
  injectPageScript("media-hook.js");
}

function collectPageMedia() {
  return AvarMedia.mergeMediaItems(
    AvarMedia.collectMediaItems(document),
    AvarMedia.collectDomVideoItems(document),
    [...hookedMediaItems.values()],
  );
}

function rememberSelectedLinks(items) {
  if (items.length > 0) {
    cachedSelectedItems = items;
    cachedSelectedAt = Date.now();
    return;
  }
  cachedSelectedItems = [];
  cachedSelectedAt = 0;
}

function readSelectedLinksFromDom() {
  if (typeof AvarMedia === "undefined") {
    return [];
  }
  return AvarMedia.collectSelectedLinkItems(document);
}

function collectSelectedLinks() {
  const items = readSelectedLinksFromDom();
  rememberSelectedLinks(items);
  return items;
}

function getSelectedLinksForContextMenu() {
  if (
    contextMenuSnapshot.length > 0 &&
    Date.now() - contextMenuSnapshotAt < CONTEXT_MENU_SNAPSHOT_MS
  ) {
    return contextMenuSnapshot;
  }

  const current = readSelectedLinksFromDom();
  if (current.length > 0) {
    return current;
  }

  if (
    cachedSelectedItems.length > 0 &&
    Date.now() - cachedSelectedAt < SELECTED_LINKS_CACHE_MS
  ) {
    return cachedSelectedItems;
  }

  return [];
}

function refreshContextMenuSnapshot() {
  const current = readSelectedLinksFromDom();
  if (current.length > 0) {
    contextMenuSnapshot = current;
    contextMenuSnapshotAt = Date.now();
    rememberSelectedLinks(current);
    return contextMenuSnapshot;
  }

  if (
    cachedSelectedItems.length > 0 &&
    Date.now() - cachedSelectedAt < SELECTED_LINKS_CACHE_MS
  ) {
    contextMenuSnapshot = cachedSelectedItems;
    contextMenuSnapshotAt = Date.now();
    return contextMenuSnapshot;
  }

  contextMenuSnapshot = [];
  contextMenuSnapshotAt = 0;
  return contextMenuSnapshot;
}

function removeSelectionWidget() {
  if (selectionWidgetHost) {
    selectionWidgetHost.remove();
    selectionWidgetHost = null;
  }
  widgetDragState = null;
}

function clampWidgetPosition(left, top, width, height) {
  const clampedLeft = Math.max(
    WIDGET_VIEWPORT_PADDING_PX,
    Math.min(left, window.innerWidth - width - WIDGET_VIEWPORT_PADDING_PX),
  );
  const clampedTop = Math.max(
    WIDGET_VIEWPORT_PADDING_PX,
    Math.min(top, window.innerHeight - height - WIDGET_VIEWPORT_PADDING_PX),
  );
  return { left: clampedLeft, top: clampedTop };
}

function clampWidgetOpacity(value) {
  const parsed = Number(value);
  const opacity = Number.isFinite(parsed) ? parsed : MAX_SELECTION_WIDGET_OPACITY;
  return Math.max(
    MIN_SELECTION_WIDGET_OPACITY,
    Math.min(MAX_SELECTION_WIDGET_OPACITY, Math.round(opacity)),
  );
}

function getWidgetDimensions(host) {
  const widget = host.shadowRoot?.querySelector(".widget");
  if (!widget) {
    return { width: 0, height: 0 };
  }
  return { width: widget.offsetWidth, height: widget.offsetHeight };
}

function applyWidgetOpacity(shadow) {
  const widget = shadow?.querySelector(".widget");
  if (widget) {
    widget.style.opacity = String(clampWidgetOpacity(selectionWidgetOpacity) / 100);
  }
}

function applyWidgetPosition(host, left, top) {
  const { width, height } = getWidgetDimensions(host);
  const clamped = clampWidgetPosition(left, top, width, height);
  host.style.left = `${Math.round(clamped.left)}px`;
  host.style.top = `${Math.round(clamped.top)}px`;
  return clamped;
}

function getSelectedAnchorsBoundingRect() {
  const selection = window.getSelection();
  if (!selection || selection.rangeCount === 0 || typeof AvarMedia === "undefined") {
    return null;
  }

  const ranges = AvarMedia.snapshotSelectionRanges(selection);
  if (ranges.length === 0) {
    return null;
  }

  let top = Infinity;
  let left = Infinity;
  let bottom = -Infinity;
  let right = -Infinity;
  let found = false;

  try {
    for (const range of ranges) {
      AvarMedia.forEachAnchorsNearRange(range, (anchor) => {
        const href = anchor.href;
        if (!href || href.startsWith("javascript:")) {
          return;
        }
        if (!AvarMedia.rangeContainsNode(range, anchor)) {
          return;
        }
        const rect = anchor.getBoundingClientRect();
        top = Math.min(top, rect.top);
        left = Math.min(left, rect.left);
        bottom = Math.max(bottom, rect.bottom);
        right = Math.max(right, rect.right);
        found = true;
      });
    }
  } catch {
    return null;
  }

  if (!found) {
    return null;
  }

  return {
    top,
    left,
    bottom,
    right,
    width: right - left,
    height: bottom - top,
  };
}

function unionRectBounds(target, rect) {
  if (!rect || (rect.width === 0 && rect.height === 0)) {
    return target;
  }
  if (!target) {
    return {
      top: rect.top,
      left: rect.left,
      bottom: rect.bottom,
      right: rect.right,
      width: rect.width,
      height: rect.height,
    };
  }
  const top = Math.min(target.top, rect.top);
  const left = Math.min(target.left, rect.left);
  const bottom = Math.max(target.bottom, rect.bottom);
  const right = Math.max(target.right, rect.right);
  return {
    top,
    left,
    bottom,
    right,
    width: right - left,
    height: bottom - top,
  };
}

function getSelectionAnchorRect() {
  const selection = window.getSelection();
  if (!selection || selection.rangeCount === 0 || typeof AvarMedia === "undefined") {
    return getSelectedAnchorsBoundingRect();
  }

  if (selection.isCollapsed) {
    return getSelectedAnchorsBoundingRect();
  }

  let bounds = null;
  try {
    for (const range of AvarMedia.snapshotSelectionRanges(selection)) {
      const clientRects = range.getClientRects();
      if (clientRects.length > 0) {
        for (let i = 0; i < clientRects.length; i += 1) {
          bounds = unionRectBounds(bounds, clientRects[i]);
        }
      } else {
        bounds = unionRectBounds(bounds, range.getBoundingClientRect());
      }
    }
  } catch {
    return getSelectedAnchorsBoundingRect();
  }

  return bounds || getSelectedAnchorsBoundingRect();
}

function getPointerAnchorRect() {
  const size = 1;
  return {
    top: lastPointer.y,
    left: lastPointer.x,
    bottom: lastPointer.y + size,
    right: lastPointer.x + size,
    width: size,
    height: size,
  };
}

function getNearestSelectedAnchorRect() {
  const selection = window.getSelection();
  if (!selection || selection.rangeCount === 0 || typeof AvarMedia === "undefined") {
    return null;
  }

  const ranges = AvarMedia.snapshotSelectionRanges(selection);
  if (ranges.length === 0) {
    return null;
  }

  let bestRect = null;
  let bestDistance = Infinity;

  try {
    for (const range of ranges) {
      AvarMedia.forEachAnchorsNearRange(range, (anchor) => {
        const href = anchor.href;
        if (!href || href.startsWith("javascript:")) {
          return;
        }
        if (!AvarMedia.rangeContainsNode(range, anchor)) {
          return;
        }

        const rect = anchor.getBoundingClientRect();
        const centerX = rect.left + rect.width / 2;
        const centerY = rect.top + rect.height / 2;
        const distance =
          (centerX - lastPointer.x) * (centerX - lastPointer.x) +
          (centerY - lastPointer.y) * (centerY - lastPointer.y);
        if (distance < bestDistance) {
          bestDistance = distance;
          bestRect = rect;
        }
      });
    }
  } catch {
    return null;
  }

  if (!bestRect) {
    return null;
  }

  return {
    top: bestRect.top,
    left: bestRect.left,
    bottom: bestRect.bottom,
    right: bestRect.right,
    width: bestRect.width,
    height: bestRect.height,
  };
}

function getWidgetAnchorRect() {
  return (
    getSelectionAnchorRect() ||
    getSelectedAnchorsBoundingRect() ||
    getNearestSelectedAnchorRect() ||
    getPointerAnchorRect()
  );
}

function positionSelectionWidgetNow(host) {
  if (widgetManualPosition) {
    applyWidgetPosition(host, widgetManualPosition.left, widgetManualPosition.top);
    return;
  }

  const anchor = getWidgetAnchorRect();
  const { width: widgetWidth, height: widgetHeight } = getWidgetDimensions(host);
  if (!anchor) {
    applyWidgetPosition(host, WIDGET_VIEWPORT_PADDING_PX, WIDGET_VIEWPORT_PADDING_PX);
    return;
  }
  if (widgetWidth === 0 || widgetHeight === 0) {
    requestAnimationFrame(() => {
      if (selectionWidgetHost === host) {
        positionSelectionWidgetNow(host);
      }
    });
    return;
  }

  let left = anchor.left;
  if (lastPointer.x >= anchor.left && lastPointer.x <= anchor.right) {
    left = Math.min(lastPointer.x, anchor.right - widgetWidth);
  }

  let top = anchor.bottom + WIDGET_MARGIN_PX;
  if (top + widgetHeight > window.innerHeight - WIDGET_VIEWPORT_PADDING_PX) {
    top = anchor.top - widgetHeight - WIDGET_MARGIN_PX;
  }

  const clamped = clampWidgetPosition(left, top, widgetWidth, widgetHeight);
  host.style.left = `${Math.round(clamped.left)}px`;
  host.style.top = `${Math.round(clamped.top)}px`;
}

function positionSelectionWidget(host) {
  requestAnimationFrame(() => {
    if (selectionWidgetHost === host) {
      positionSelectionWidgetNow(host);
    }
  });
}

function installWidgetDrag(shadow, host) {
  const widget = shadow.querySelector(".widget");
  const dragHandle = shadow.querySelector(".drag-handle");
  if (!widget || !dragHandle) {
    return;
  }

  dragHandle.addEventListener("mousedown", (event) => {
    event.preventDefault();
    event.stopPropagation();
    const rect = host.getBoundingClientRect();
    widgetDragState = {
      startX: event.clientX,
      startY: event.clientY,
      origLeft: rect.left,
      origTop: rect.top,
    };
    dragHandle.style.cursor = "grabbing";
    widget.style.cursor = "grabbing";
  });
}

function handleWidgetDragMove(event) {
  if (!widgetDragState || !selectionWidgetHost) {
    return;
  }

  const dx = event.clientX - widgetDragState.startX;
  const dy = event.clientY - widgetDragState.startY;
  const clamped = applyWidgetPosition(
    selectionWidgetHost,
    widgetDragState.origLeft + dx,
    widgetDragState.origTop + dy,
  );
  widgetManualPosition = clamped;
}

function handleWidgetDragEnd() {
  if (!widgetDragState) {
    return;
  }
  widgetDragState = null;
  const shadow = selectionWidgetHost?.shadowRoot;
  const widget = shadow?.querySelector(".widget");
  const dragHandle = shadow?.querySelector(".drag-handle");
  if (dragHandle) {
    dragHandle.style.cursor = "grab";
  }
  if (widget) {
    widget.style.cursor = "";
  }
}

function ensureSelectionWidget() {
  if (selectionWidgetHost) {
    return selectionWidgetHost;
  }

  const host = document.createElement("div");
  host.id = "avar-selection-widget-host";
  host.style.cssText =
    "position: fixed; z-index: 2147483646; left: 0; top: 0; display: block; margin: 0; padding: 0; border: 0;";

  const shadow = host.attachShadow({ mode: "closed" });
  shadow.innerHTML = `
    <style>
      :host {
        display: block;
        font-family: system-ui, sans-serif;
      }
      .widget {
        display: flex;
        align-items: center;
        gap: 8px;
        padding: 8px 8px 8px 4px;
        border-radius: 8px;
        background: #0f172a;
        color: #e2e8f0;
        border: 1px solid #334155;
        box-shadow: 0 4px 16px rgba(0, 0, 0, 0.35);
        font-size: 13px;
        line-height: 1.2;
      }
      .drag-handle {
        margin: 0;
        padding: 4px 2px;
        border: none;
        border-radius: 4px;
        background: transparent;
        color: #64748b;
        font: inherit;
        line-height: 1;
        cursor: grab;
        user-select: none;
        touch-action: none;
        letter-spacing: -0.15em;
      }
      .drag-handle:hover {
        color: #94a3b8;
        background: #1e293b;
      }
      .count {
        color: #94a3b8;
        white-space: nowrap;
      }
      .download-btn {
        margin: 0;
        padding: 6px 12px;
        border: none;
        border-radius: 6px;
        background: #38bdf8;
        color: #0f172a;
        font: inherit;
        font-weight: 600;
        cursor: pointer;
        white-space: nowrap;
      }
      .download-btn:hover {
        background: #7dd3fc;
      }
      .download-btn:disabled {
        opacity: 0.6;
        cursor: default;
      }
      .close-btn {
        margin: 0;
        margin-inline-start: 2px;
        padding: 2px 6px;
        border: none;
        border-radius: 6px;
        background: transparent;
        color: #94a3b8;
        font: inherit;
        font-size: 16px;
        line-height: 1;
        cursor: pointer;
      }
      .close-btn:hover {
        background: #1e293b;
        color: #e2e8f0;
      }
    </style>
    <div class="widget" role="region" aria-label="Avar download selection">
      <button type="button" class="drag-handle" id="dragHandle" aria-label="Move" title="Drag to move">⋮⋮</button>
      <span class="count" id="count"></span>
      <button type="button" class="download-btn" id="downloadBtn">Download all</button>
      <button type="button" class="close-btn" id="closeBtn" aria-label="Dismiss">×</button>
    </div>
  `;

  shadow.getElementById("downloadBtn").addEventListener("click", () => {
    void downloadSelectedLinks(shadow);
  });
  shadow.getElementById("closeBtn").addEventListener("click", () => {
    widgetDismissed = true;
    widgetManualPosition = null;
    removeSelectionWidget();
  });

  installWidgetDrag(shadow, host);

  document.documentElement.appendChild(host);
  selectionWidgetHost = host;
  return host;
}

function buildContentDownloadItem(item) {
  const linkName = AvarMedia.itemDisplayFilename(item, document.title);
  return {
    url: item.url,
    streamKind: item.kind,
    filename: linkName,
    linkName,
    fileType: AvarMedia.classifyMediaCategory(item),
    referer: location.href,
    originalUrl: location.href,
  };
}

async function downloadSelectedLinks(shadow) {
  if (!ensureExtensionContext()) {
    return;
  }
  const items = collectSelectedLinks();
  if (items.length === 0) {
    return;
  }

  const button = shadow.getElementById("downloadBtn");
  button.disabled = true;
  const previousLabel = button.textContent;
  button.textContent = "Opening…";

  try {
    const response = await api.runtime.sendMessage({
      type: "avar-open-downloads",
      items: items.map((item) => buildContentDownloadItem(item)),
      pageUrl: location.href,
      pageTitle: document.title,
    });
    button.textContent = response?.ok ? "Opened in Avar" : "Failed";
  } catch {
    ensureExtensionContext();
    button.textContent = "Failed";
  }

  setTimeout(() => {
    button.disabled = false;
    button.textContent = previousLabel;
  }, 2000);
}

function updateSelectionWidget() {
  if (!ensureExtensionContext()) {
    return;
  }
  const items = collectSelectedLinks();
  const selectionKey = items.map((item) => item.url).join("\n");
  if (selectionKey !== lastWidgetSelectionKey) {
    widgetDismissed = false;
    widgetManualPosition = null;
    lastWidgetLinkCount = items.length;
    lastWidgetSelectionKey = selectionKey;
  } else if (items.length !== lastWidgetLinkCount) {
    widgetDismissed = false;
    widgetManualPosition = null;
    lastWidgetLinkCount = items.length;
  }

  const shouldShow =
    showSelectionWidget && items.length >= MIN_SELECTED_LINKS_FOR_WIDGET && !widgetDismissed;

  if (!shouldShow) {
    removeSelectionWidget();
    return;
  }

  const host = ensureSelectionWidget();
  const shadow = host.shadowRoot;
  if (!shadow) {
    return;
  }

  const countEl = shadow.getElementById("count");
  if (countEl) {
    countEl.textContent = `${items.length} link(s) selected`;
  }

  applyWidgetOpacity(shadow);
  positionSelectionWidget(host);
}

async function loadWidgetSetting() {
  if (!ensureExtensionContext()) {
    return;
  }
  const stored = await api.storage.local.get(["showSelectionWidget", "selectionWidgetOpacity"]);
  showSelectionWidget = stored.showSelectionWidget !== false;
  selectionWidgetOpacity = clampWidgetOpacity(stored.selectionWidgetOpacity);
  updateSelectionWidget();
}

function scheduleSelectionWidgetUpdate() {
  if (!ensureExtensionContext()) {
    return;
  }
  const count = collectSelectedLinks().length;
  void sendRuntimeMessage({ type: "avar-selection-changed", count });

  if (selectionChangeTimer) {
    clearTimeout(selectionChangeTimer);
  }
  selectionChangeTimer = setTimeout(() => {
    selectionChangeTimer = null;
    if (!ensureExtensionContext()) {
      return;
    }
    updateSelectionWidget();
  }, SELECTION_WIDGET_DEBOUNCE_MS);
}

api.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (!ensureExtensionContext()) {
    return false;
  }
  if (message?.type !== "avar-get-page-media") {
    return false;
  }

  if (typeof AvarMedia === "undefined") {
    sendResponse({ urls: [], items: [], selectedItems: [] });
    return true;
  }

  const items = collectPageMedia();
  const selectedItems = message.forContextMenu
    ? getSelectedLinksForContextMenu()
    : collectSelectedLinks();
  sendResponse({
    urls: items.map((item) => item.url),
    items,
    selectedItems,
    pageTitle: document.title || "",
  });
  return true;
});

document.addEventListener("selectionchange", scheduleSelectionWidgetUpdate);

document.addEventListener(
  "mousedown",
  (event) => {
    lastPointer = { x: event.clientX, y: event.clientY };
  },
  true,
);

document.addEventListener(
  "mousemove",
  (event) => {
    if (event.buttons !== 0) {
      lastPointer = { x: event.clientX, y: event.clientY };
    }
    handleWidgetDragMove(event);
  },
  true,
);

document.addEventListener(
  "mouseup",
  (event) => {
    lastPointer = { x: event.clientX, y: event.clientY };
    handleWidgetDragEnd();
  },
  true,
);

function repositionSelectionWidgetIfVisible() {
  if (!selectionWidgetHost || widgetManualPosition) {
    return;
  }
  positionSelectionWidgetNow(selectionWidgetHost);
}

window.addEventListener("scroll", repositionSelectionWidgetIfVisible, true);
window.addEventListener("resize", repositionSelectionWidgetIfVisible);

api.storage.onChanged.addListener((changes, area) => {
  if (!ensureExtensionContext()) {
    return;
  }
  if (area !== "local") {
    return;
  }
  let shouldUpdate = false;
  if (changes.showSelectionWidget) {
    showSelectionWidget = changes.showSelectionWidget.newValue !== false;
    shouldUpdate = true;
  }
  if (changes.selectionWidgetOpacity) {
    selectionWidgetOpacity = clampWidgetOpacity(changes.selectionWidgetOpacity.newValue);
    shouldUpdate = true;
  }
  if (shouldUpdate) {
    updateSelectionWidget();
  }
});

document.addEventListener(
  "contextmenu",
  () => {
    if (!ensureExtensionContext()) {
      return;
    }
    const snapshot = refreshContextMenuSnapshot();
    void sendRuntimeMessage({ type: "avar-selection-changed", count: snapshot.length });
    updateSelectionWidget();
  },
  true,
);

installPageHooks();
void (async () => {
  if (!ensureExtensionContext()) {
    return;
  }
  await loadWidgetSetting();
  const count = collectSelectedLinks().length;
  void sendRuntimeMessage({ type: "avar-selection-changed", count });
})();
