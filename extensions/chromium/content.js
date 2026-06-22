const api = typeof browser !== "undefined" ? browser : chrome;

const MIN_SELECTED_LINKS_FOR_WIDGET = 2;
const SELECTION_WIDGET_DEBOUNCE_MS = 100;

/** @type {Map<string, object>} */
const hookedMediaItems = new Map();

let showSelectionWidget = false;
/** @type {HTMLElement | null} */
let selectionWidgetHost = null;
let selectionChangeTimer = null;

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

function collectSelectedLinks() {
  if (typeof AvarMedia === "undefined") {
    return [];
  }
  return AvarMedia.collectSelectedLinkItems(document);
}

function removeSelectionWidget() {
  if (selectionWidgetHost) {
    selectionWidgetHost.remove();
    selectionWidgetHost = null;
  }
}

function ensureSelectionWidget() {
  if (selectionWidgetHost) {
    return selectionWidgetHost;
  }

  const host = document.createElement("div");
  host.id = "avar-selection-widget-host";
  host.style.cssText = "all: initial; position: fixed; z-index: 2147483646;";

  const shadow = host.attachShadow({ mode: "closed" });
  shadow.innerHTML = `
    <style>
      :host {
        all: initial;
        font-family: system-ui, sans-serif;
      }
      .widget {
        position: fixed;
        right: 16px;
        bottom: 16px;
        display: flex;
        align-items: center;
        gap: 8px;
        padding: 8px 12px;
        border-radius: 8px;
        background: #0f172a;
        color: #e2e8f0;
        border: 1px solid #334155;
        box-shadow: 0 4px 16px rgba(0, 0, 0, 0.35);
        font-size: 13px;
        line-height: 1.2;
      }
      .count {
        color: #94a3b8;
        white-space: nowrap;
      }
      button {
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
      button:hover {
        background: #7dd3fc;
      }
      button:disabled {
        opacity: 0.6;
        cursor: default;
      }
    </style>
    <div class="widget" role="region" aria-label="Avar download selection">
      <span class="count" id="count"></span>
      <button type="button" id="downloadBtn">Download all</button>
    </div>
  `;

  shadow.getElementById("downloadBtn").addEventListener("click", () => {
    void downloadSelectedLinks(shadow);
  });

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
    button.textContent = "Failed";
  }

  setTimeout(() => {
    button.disabled = false;
    button.textContent = previousLabel;
  }, 2000);
}

function updateSelectionWidget() {
  const items = collectSelectedLinks();
  const shouldShow = showSelectionWidget && items.length >= MIN_SELECTED_LINKS_FOR_WIDGET;

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
}

async function loadWidgetSetting() {
  const stored = await api.storage.local.get(["showSelectionWidget"]);
  showSelectionWidget = Boolean(stored.showSelectionWidget);
  updateSelectionWidget();
}

function scheduleSelectionWidgetUpdate() {
  if (selectionChangeTimer) {
    clearTimeout(selectionChangeTimer);
  }
  selectionChangeTimer = setTimeout(() => {
    selectionChangeTimer = null;
    updateSelectionWidget();
    notifySelectionChanged();
  }, SELECTION_WIDGET_DEBOUNCE_MS);
}

function notifySelectionChanged() {
  const count = collectSelectedLinks().length;
  void api.runtime.sendMessage({ type: "avar-selection-changed", count }).catch(() => {});
}

api.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  if (message?.type !== "avar-get-page-media") {
    return false;
  }

  if (typeof AvarMedia === "undefined") {
    sendResponse({ urls: [], items: [], selectedItems: [] });
    return true;
  }

  const items = collectPageMedia();
  const selectedItems = collectSelectedLinks();
  sendResponse({
    urls: items.map((item) => item.url),
    items,
    selectedItems,
    pageTitle: document.title || "",
  });
  return true;
});

document.addEventListener("selectionchange", scheduleSelectionWidgetUpdate);

api.storage.onChanged.addListener((changes, area) => {
  if (area !== "local" || !changes.showSelectionWidget) {
    return;
  }
  showSelectionWidget = Boolean(changes.showSelectionWidget.newValue);
  updateSelectionWidget();
});

installPageHooks();
void loadWidgetSetting();
notifySelectionChanged();
