const DEFAULT_BRIDGE = "http://127.0.0.1:18766";
const DEFAULT_MEDIA_FILTER = "all";
const DEFAULT_MEDIA_SORT = "type";
const DEFAULT_POPUP_WIDTH = 420;
const DEFAULT_POPUP_HEIGHT = 560;
const POPUP_WIDTH_MIN = 320;
const POPUP_WIDTH_MAX = 760;
const POPUP_HEIGHT_MIN = 400;
const POPUP_HEIGHT_MAX = 600;

let verticalResizeEnabled = false;

const api = typeof browser !== "undefined" ? browser : chrome;

const bridgeUrlInput = document.getElementById("bridgeUrl");
const statusEl = document.getElementById("status");
const bridgeStatusEl = document.getElementById("bridgeStatus");
const bridgeStatusLabel = document.getElementById("bridgeStatusLabel");
const mediaList = document.getElementById("mediaList");
const selectedSection = document.getElementById("selectedSection");
const selectedLinksList = document.getElementById("selectedLinksList");
const downloadSelectedBtn = document.getElementById("downloadSelected");
const downloadAllBtn = document.getElementById("downloadAll");
const mainView = document.getElementById("mainView");
const settingsView = document.getElementById("settingsView");
const settingsBtn = document.getElementById("settingsBtn");
const settingsBackBtn = document.getElementById("settingsBackBtn");
const refreshBtn = document.getElementById("refreshBtn");
const defaultQueueSelect = document.getElementById("defaultQueue");
const defaultMediaFilterSelect = document.getElementById("defaultMediaFilter");
const showSelectionWidgetInput = document.getElementById("showSelectionWidget");
const mediaTypeFilterSelect = document.getElementById("mediaTypeFilter");
const mediaSortSelect = document.getElementById("mediaSort");
const queueManagementEl = document.getElementById("queueManagement");

let lastMediaItems = [];
let lastSelectedItems = [];
let knownQueues = [];
let pageReferer = null;
let pageTitle = "";
let bridgeConnected = false;
let hlsExpandGeneration = 0;
const probedSizes = new Map();
const probedFilenames = new Map();
const probingUrls = new Set();

const DOWNLOAD_ICON =
  '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 20h14v-2H5v2zm7-18v10h3l-4 4-4-4h3V2z"/></svg>';

const COPY_ICON =
  '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M16 1H4c-1.1 0-2 .9-2 2v14h2V3h12V1zm3 4H8c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h11c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2zm0 16H8V7h11v14z"/></svg>';

const MEDIA_TYPE_ICONS = {
  video:
    '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M17 10.5V7a1 1 0 0 0-1-1H4a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-3.5l4 2.5v-9l-4 2.5z"/></svg>',
  audio:
    '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 3v10.55A4 4 0 1 0 14 17V7h4V3h-6z"/></svg>',
  image:
    '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 19V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2zM8.5 13.5l2.5 3.01L14.5 12l4.5 6H5l3.5-4.5z"/></svg>',
  binary:
    '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/></svg>',
};

function setStatus(text) {
  statusEl.textContent = text;
}

function setBridgeConnected(connected) {
  bridgeStatusEl.classList.toggle("status-dot--ok", connected);
  bridgeStatusLabel.textContent = connected ? "Connected" : "Disconnected";
  bridgeStatusLabel.classList.toggle("connection-status__label--ok", connected);
  bridgeStatusEl.title = connected
    ? "Connected to Avar bridge"
    : "Cannot reach Avar bridge";
}

function getActiveMediaFilter() {
  return mediaTypeFilterSelect?.value || DEFAULT_MEDIA_FILTER;
}

function getActiveMediaSort() {
  return mediaSortSelect?.value || DEFAULT_MEDIA_SORT;
}

function displayFilename(item) {
  if (probedFilenames.has(item.url)) {
    const probed = probedFilenames.get(item.url);
    if (probed) {
      return probed;
    }
  }
  return AvarMedia.itemDisplayFilename(item, pageTitle);
}

function getFilteredMediaItems(items) {
  return AvarMedia.filterMediaItems(items, getActiveMediaFilter());
}

function getVisibleMediaItems(items) {
  const listed = items.filter((item) => AvarMedia.shouldListMediaItem(item));
  const filtered = AvarMedia.filterMediaItems(listed, getActiveMediaFilter());
  return AvarMedia.sortMediaItemsByMode(filtered, getActiveMediaSort(), getItemSize);
}

function appendStreamBadge(nameEl, item) {
  if (item.hlsLabel) {
    const badge = document.createElement("span");
    badge.className = "media-kind";
    badge.textContent = item.hlsLabel;
    nameEl.appendChild(badge);
    return;
  }
  if (item.kind === "hls" || item.kind === "dash") {
    const badge = document.createElement("span");
    badge.className = "media-kind";
    badge.textContent = item.kind;
    nameEl.appendChild(badge);
  }
}

function updateScanStatus(totalCount) {
  const filter = getActiveMediaFilter();
  const filtered = AvarMedia.filterMediaItems(lastMediaItems, filter);
  const selectedCount = lastSelectedItems.length;
  let status = "";
  if (selectedCount > 0) {
    status = `${selectedCount} selected link(s). `;
  }
  if (filter === "all" || filtered.length === totalCount) {
    status += `${totalCount} media URL(s) found.`;
  } else {
    status += `${filtered.length} of ${totalCount} media URL(s) shown.`;
  }
  setStatus(status.trim());
}

function queueNameForId(queueId) {
  if (!queueId) {
    return null;
  }
  const queue = knownQueues.find((item) => item.id === queueId);
  return queue?.name || null;
}

function applyPopupSize(width, height) {
  const nextWidth = Math.max(
    POPUP_WIDTH_MIN,
    Math.min(POPUP_WIDTH_MAX, Number(width) || DEFAULT_POPUP_WIDTH),
  );
  let nextHeight = DEFAULT_POPUP_HEIGHT;
  if (verticalResizeEnabled) {
    nextHeight = Math.max(
      POPUP_HEIGHT_MIN,
      Math.min(POPUP_HEIGHT_MAX, Number(height) || DEFAULT_POPUP_HEIGHT),
    );
  }

  document.documentElement.style.width = `${nextWidth}px`;
  document.documentElement.style.height = `${nextHeight}px`;
}

function persistPopupSize(width, height) {
  const payload = { popupWidth: Math.round(width) };
  if (verticalResizeEnabled && typeof height === "number") {
    payload.popupHeight = Math.round(height);
  }
  void api.storage.local.set(payload);
}

function hasOuterDocumentScroll() {
  return (
    document.documentElement.scrollHeight > document.documentElement.clientHeight + 1 ||
    document.body.scrollHeight > document.body.clientHeight + 1
  );
}

function browserHonorsPopupHeight(requestedHeight) {
  void document.documentElement.offsetHeight;
  return document.documentElement.clientHeight >= requestedHeight - 2;
}

function initVerticalResizeProbe(storedHeight) {
  const bottomHandle = document.querySelector(".resize-handle--bottom");
  const width = document.documentElement.getBoundingClientRect().width;

  document.documentElement.style.height = `${POPUP_HEIGHT_MAX}px`;
  void document.documentElement.offsetHeight;

  const honored = browserHonorsPopupHeight(POPUP_HEIGHT_MAX);
  const scrolls = hasOuterDocumentScroll();
  verticalResizeEnabled = honored && !scrolls;

  if (bottomHandle) {
    bottomHandle.hidden = !verticalResizeEnabled;
    bottomHandle.setAttribute("aria-hidden", verticalResizeEnabled ? "false" : "true");
  }

  if (!verticalResizeEnabled) {
    applyPopupSize(width, DEFAULT_POPUP_HEIGHT);
    return;
  }

  applyPopupSize(width, storedHeight ?? DEFAULT_POPUP_HEIGHT);
}

function installPopupResizePersistence() {
  let saveTimer = null;
  const observer = new ResizeObserver((entries) => {
    const entry = entries[0];
    if (!entry) {
      return;
    }
    const width = Math.round(entry.contentRect.width);
    const height = Math.round(entry.contentRect.height);
    if (saveTimer) {
      clearTimeout(saveTimer);
    }
    saveTimer = setTimeout(() => {
      persistPopupSize(width, verticalResizeEnabled ? height : undefined);
    }, 200);
  });
  observer.observe(document.documentElement);
}

function installPopupResizeHandles() {
  let drag = null;

  function onPointerMove(event) {
    if (!drag) {
      return;
    }
    const dx = event.clientX - drag.startX;
    const dy = event.clientY - drag.startY;
    let width = drag.startWidth;
    let height = drag.startHeight;

    if (drag.axis === "w") {
      width = drag.startWidth - dx;
    } else if (drag.axis === "s" && verticalResizeEnabled) {
      height = drag.startHeight + dy;
    }

    applyPopupSize(width, height);

    if (drag.axis === "s" && verticalResizeEnabled) {
      if (!browserHonorsPopupHeight(height) || hasOuterDocumentScroll()) {
        applyPopupSize(width, drag.startHeight);
      }
    }
  }

  function onPointerUp() {
    if (!drag) {
      return;
    }
    const rect = document.documentElement.getBoundingClientRect();
    persistPopupSize(rect.width, verticalResizeEnabled ? rect.height : undefined);
    drag = null;
    document.removeEventListener("pointermove", onPointerMove);
    document.removeEventListener("pointerup", onPointerUp);
  }

  for (const handle of document.querySelectorAll(".resize-handle")) {
    if (handle.hidden) {
      continue;
    }
    handle.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      const rect = document.documentElement.getBoundingClientRect();
      drag = {
        axis: handle.dataset.resize,
        startX: event.clientX,
        startY: event.clientY,
        startWidth: rect.width,
        startHeight: rect.height,
      };
      document.addEventListener("pointermove", onPointerMove);
      document.addEventListener("pointerup", onPointerUp);
    });
  }
}

function showSettingsView() {
  mainView.hidden = true;
  settingsView.hidden = false;
  settingsBtn.setAttribute("aria-pressed", "true");
  settingsBtn.title = "Back to media list";
  void loadQueues();
}

function showMainView() {
  settingsView.hidden = true;
  mainView.hidden = false;
  settingsBtn.setAttribute("aria-pressed", "false");
  settingsBtn.title = "Settings";
}

function isSettingsOpen() {
  return !settingsView.hidden;
}

function populateDefaultQueueOptions(selectedId) {
  const previous = selectedId ?? defaultQueueSelect.value;
  defaultQueueSelect.innerHTML = '<option value="">Default queue</option>';
  for (const queue of knownQueues) {
    const option = document.createElement("option");
    option.value = queue.id;
    option.textContent = queue.name;
    defaultQueueSelect.appendChild(option);
  }
  if (previous && knownQueues.some((queue) => queue.id === previous)) {
    defaultQueueSelect.value = previous;
  }
}

function renderQueueManagement(queues) {
  queueManagementEl.innerHTML = "";
  if (!queues.length) {
    const empty = document.createElement("p");
    empty.className = "queue-management__empty";
    empty.textContent = "No queues found.";
    queueManagementEl.appendChild(empty);
    return;
  }

  for (const queue of queues) {
    const row = document.createElement("div");
    row.className = "queue-row";

    const name = document.createElement("span");
    name.className = "queue-row__name";
    name.textContent = queue.name;
    name.title = queue.description || queue.name;

    const status = document.createElement("span");
    status.className = queue.running
      ? "queue-row__status queue-row__status--running"
      : "queue-row__status";
    status.textContent = queue.running ? "Running" : "Stopped";

    const action = document.createElement("button");
    action.type = "button";
    action.className = "queue-row__btn";
    action.textContent = queue.running ? "Stop" : "Start";
    action.addEventListener("click", async () => {
      const wasRunning = queue.running;
      const type = wasRunning ? "avar-queue-stop" : "avar-queue-start";
      const response = await api.runtime.sendMessage({ type, queueId: queue.id });
      if (response?.ok) {
        await loadQueues();
        setStatus(wasRunning ? `Stopped queue: ${queue.name}` : `Started queue: ${queue.name}`);
      } else {
        setStatus(response?.error || "Queue action failed.");
      }
    });

    row.appendChild(name);
    row.appendChild(status);
    row.appendChild(action);
    queueManagementEl.appendChild(row);
  }
}

async function loadQueues() {
  const response = await api.runtime.sendMessage({ type: "avar-list-queues" });
  if (!response?.ok) {
    knownQueues = [];
    renderQueueManagement([]);
    populateDefaultQueueOptions();
    return;
  }

  knownQueues = (response.queues || []).map((queue) => ({
    id: String(queue.id ?? ""),
    name: String(queue.name ?? ""),
    description: queue.description ? String(queue.description) : "",
    running: Boolean(queue.started ?? queue.running),
  }));
  renderQueueManagement(knownQueues);
  populateDefaultQueueOptions(defaultQueueSelect.value);
}

function getItemSize(item) {
  if (typeof item.size === "number" && item.size >= 0) {
    return item.size;
  }
  if (probedSizes.has(item.url)) {
    return probedSizes.get(item.url);
  }
  return undefined;
}

function shouldProbeItemSize(item) {
  if (!bridgeConnected) {
    return false;
  }
  if (item.kind === "hls" || item.kind === "dash") {
    return false;
  }
  return getItemSize(item) === undefined;
}

function findMediaItemSubelement(url, subselector) {
  for (const root of [selectedLinksList, mediaList]) {
    const el = root.querySelector(`.media-item[data-url="${CSS.escape(url)}"] ${subselector}`);
    if (el) {
      return el;
    }
  }
  return null;
}

function findMediaItem(url) {
  return lastSelectedItems.find((entry) => entry.url === url) || lastMediaItems.find((entry) => entry.url === url);
}

function updateItemSizeElement(url) {
  const sizeEl = findMediaItemSubelement(url, ".media-item__size");
  if (!sizeEl) {
    return;
  }
  const item = findMediaItem(url);
  if (!item) {
    return;
  }
  const size = getItemSize(item);
  sizeEl.classList.remove("media-item__size--pending");
  if (size === undefined) {
    sizeEl.textContent = "…";
    sizeEl.classList.add("media-item__size--pending");
    sizeEl.title = "Checking file size";
    return;
  }
  if (size === null) {
    sizeEl.textContent = "—";
    sizeEl.title = "Size unknown";
    return;
  }
  sizeEl.textContent = AvarMedia.formatFileSize(size);
  sizeEl.title = "File size";
}

async function probeItemSize(item) {
  if (!shouldProbeItemSize(item) || probingUrls.has(item.url)) {
    return;
  }

  probingUrls.add(item.url);
  updateItemSizeElement(item.url);

  const response = await api.runtime.sendMessage({
    type: "avar-probe-size",
    url: item.url,
    referer: pageReferer,
  });

  probingUrls.delete(item.url);
  if (!response?.ok) {
    probedSizes.set(item.url, null);
    updateItemSizeElement(item.url);
    return;
  }

  const size =
    typeof response?.size === "number" && response.size >= 0 ? response.size : null;
  probedSizes.set(item.url, size);
  if (typeof response?.filename === "string" && response.filename.trim()) {
    probedFilenames.set(item.url, response.filename.trim());
  }
  if (getActiveMediaSort() !== DEFAULT_MEDIA_SORT) {
    renderMediaList(lastMediaItems);
    return;
  }
  updateItemSizeElement(item.url);
  updateItemNameElement(item.url);
}

function queueSizeProbes(items) {
  for (const item of items) {
    if (shouldProbeItemSize(item)) {
      void probeItemSize(item);
    }
  }
}

function createSizeElement(item) {
  const sizeEl = document.createElement("span");
  sizeEl.className = "media-item__size";
  const size = getItemSize(item);

  if (size === undefined && shouldProbeItemSize(item)) {
    sizeEl.textContent = "…";
    sizeEl.classList.add("media-item__size--pending");
    sizeEl.title = "Checking file size";
  } else if (size === null || size === undefined) {
    sizeEl.textContent = "—";
    sizeEl.title = item.kind === "hls" || item.kind === "dash" ? "Stream size unknown" : "Size unknown";
  } else {
    sizeEl.textContent = AvarMedia.formatFileSize(size);
    sizeEl.title = "File size";
  }

  return sizeEl;
}

function updateItemNameElement(url) {
  const nameEl = findMediaItemSubelement(url, ".media-item__name");
  if (!nameEl) {
    return;
  }
  const item = findMediaItem(url);
  if (!item) {
    return;
  }
  const filename = displayFilename(item);
  nameEl.textContent = filename;
  appendStreamBadge(nameEl, item);
  nameEl.title = filename;
}

function buildBatchItem(item) {
  const linkName = AvarMedia.itemDisplayFilename(item, pageTitle);
  const probedFilename = probedFilenames.get(item.url);
  const filename =
    typeof probedFilename === "string" && probedFilename.trim()
      ? probedFilename.trim()
      : linkName;
  return {
    url: item.url,
    streamKind: item.kind,
    filename,
    linkName,
    fileType: AvarMedia.classifyMediaCategory(item),
    fileSize: getItemSize(item) ?? null,
    originalUrl: pageReferer,
    referer: pageReferer,
  };
}

async function openBatchAddDialog(items) {
  if (!bridgeConnected) {
    setStatus("Cannot reach Avar bridge.");
    return;
  }
  if (!items.length) {
    return;
  }

  const response = await api.runtime.sendMessage({
    type: "avar-open-batch-add",
    items: items.map(buildBatchItem),
    pageUrl: pageReferer,
    pageTitle,
    defaultQueueId: defaultQueueSelect.value || null,
  });
  if (response?.ok) {
    setStatus(`Review ${items.length} download(s) in Avar.`);
  } else {
    setStatus(response?.error || "Failed to open batch add.");
  }
}

async function openSingleAddDialog(item) {
  if (!bridgeConnected) {
    setStatus("Cannot reach Avar bridge.");
    return;
  }

  const response = await api.runtime.sendMessage({
    type: "avar-open-add-download",
    item: buildBatchItem(item),
    pageUrl: pageReferer,
    pageTitle,
    defaultQueueId: defaultQueueSelect.value || null,
  });
  if (response?.ok) {
    setStatus("Review download in Avar.");
  } else {
    setStatus(response?.error || "Failed to open add download.");
  }
}

async function openDownloadDialog(items) {
  if (!bridgeConnected) {
    setStatus("Cannot reach Avar bridge.");
    return;
  }
  if (!items.length) {
    return;
  }
  if (items.length === 1) {
    await openSingleAddDialog(items[0]);
    return;
  }
  await openBatchAddDialog(items);
}

function createDownloadButton(item) {
  const btn = document.createElement("button");
  btn.type = "button";
  btn.className = "media-item__download";
  btn.setAttribute("aria-label", "Download");
  btn.title = bridgeConnected ? "Review in Avar" : "Connect to Avar to download";
  btn.disabled = !bridgeConnected;
  btn.innerHTML = DOWNLOAD_ICON;
  btn.addEventListener("click", async () => {
    if (lastSelectedItems.length > 1) {
      await openBatchAddDialog(lastSelectedItems);
      return;
    }
    await openSingleAddDialog(item);
  });
  return btn;
}

function createCopyButton(url) {
  const btn = document.createElement("button");
  btn.type = "button";
  btn.className = "media-item__copy icon-btn";
  btn.setAttribute("aria-label", "Copy link");
  btn.title = "Copy link";
  btn.innerHTML = COPY_ICON;
  btn.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(url);
      setStatus("Link copied.");
    } catch {
      setStatus("Could not copy link.");
    }
  });
  return btn;
}

function appendMediaItemRow(listRoot, item) {
  const li = document.createElement("li");
  li.className = "media-item";
  li.dataset.url = item.url;

  const category = AvarMedia.classifyMediaCategory(item);
  const typeIcon = document.createElement("span");
  typeIcon.className = "media-item__type-icon";
  typeIcon.title = category;
  typeIcon.innerHTML = MEDIA_TYPE_ICONS[category] || MEDIA_TYPE_ICONS.binary;

  const typeSep = document.createElement("span");
  typeSep.className = "media-item__sep";
  typeSep.setAttribute("aria-hidden", "true");

  const info = document.createElement("div");
  info.className = "media-item__info";

  const name = document.createElement("div");
  name.className = "media-item__name";
  const filename = displayFilename(item);
  name.textContent = filename;
  name.title = filename;
  appendStreamBadge(name, item);

  const metaRow = document.createElement("div");
  metaRow.className = "media-item__meta";
  metaRow.appendChild(createSizeElement(item));

  const urlRow = document.createElement("div");
  urlRow.className = "media-item__url-row";
  urlRow.appendChild(createCopyButton(item.url));

  const urlLine = document.createElement("span");
  urlLine.className = "media-item__url";
  urlLine.textContent = AvarMedia.formatDisplayUrl(item.url);
  urlLine.title = item.url;
  urlRow.appendChild(urlLine);

  metaRow.appendChild(urlRow);

  info.appendChild(name);
  info.appendChild(metaRow);

  const actions = document.createElement("div");
  actions.className = "media-item__actions";

  const actionSep = document.createElement("span");
  actionSep.className = "media-item__sep";
  actionSep.setAttribute("aria-hidden", "true");

  actions.appendChild(actionSep);
  actions.appendChild(createDownloadButton(item));

  li.appendChild(typeIcon);
  li.appendChild(typeSep);
  li.appendChild(info);
  li.appendChild(actions);
  listRoot.appendChild(li);
}

function renderSelectedLinksList(items) {
  lastSelectedItems = items;
  selectedLinksList.innerHTML = "";

  if (items.length === 0) {
    selectedSection.setAttribute("hidden", "");
    downloadSelectedBtn.hidden = true;
    return;
  }

  selectedSection.removeAttribute("hidden");
  downloadSelectedBtn.hidden = false;

  for (const item of items) {
    appendMediaItemRow(selectedLinksList, item);
  }

  queueSizeProbes(items);
}

function renderMediaList(items) {
  lastMediaItems = items;
  const selectedUrls = new Set(lastSelectedItems.map((item) => item.url));
  const visibleItems = getVisibleMediaItems(lastMediaItems).filter((item) => !selectedUrls.has(item.url));
  mediaList.innerHTML = "";

  for (const item of visibleItems) {
    appendMediaItemRow(mediaList, item);
  }

  downloadAllBtn.hidden = visibleItems.length === 0;
  updateScanStatus(lastMediaItems.length);
  queueSizeProbes(visibleItems);
}

async function refreshBridgeStatus() {
  const wasConnected = bridgeConnected;
  const response = await api.runtime.sendMessage({ type: "avar-ping-bridge" });
  bridgeConnected = Boolean(response?.ok);
  setBridgeConnected(bridgeConnected);
  if (response?.bridgeUrl && bridgeUrlInput.value !== response.bridgeUrl) {
    bridgeUrlInput.value = response.bridgeUrl;
  }
  if (wasConnected !== bridgeConnected && (lastMediaItems.length > 0 || lastSelectedItems.length > 0)) {
    if (lastSelectedItems.length > 0) {
      renderSelectedLinksList(lastSelectedItems);
    }
    renderMediaList(lastMediaItems);
    if (bridgeConnected) {
      void expandHlsInBackground(lastMediaItems);
      queueSizeProbes(getVisibleMediaItems(lastMediaItems));
    }
  }
  return response;
}

async function loadConfig() {
  const stored = await api.storage.local.get([
    "bridgeUrl",
    "guiUrl",
    "daemonUrl",
    "defaultQueueId",
    "defaultMediaFilter",
    "mediaSort",
    "popupWidth",
    "popupHeight",
    "showSelectionWidget",
  ]);
  bridgeUrlInput.value =
    AvarExtensionProtocol.normalizeBridgeUrl(
      stored.bridgeUrl || stored.guiUrl || stored.daemonUrl || DEFAULT_BRIDGE,
    );

  const defaultFilter = stored.defaultMediaFilter || DEFAULT_MEDIA_FILTER;
  defaultMediaFilterSelect.value = defaultFilter;
  mediaTypeFilterSelect.value = defaultFilter;
  mediaSortSelect.value = stored.mediaSort || DEFAULT_MEDIA_SORT;
  showSelectionWidgetInput.checked = Boolean(stored.showSelectionWidget);

  applyPopupSize(stored.popupWidth, stored.popupHeight);
  installPopupResizePersistence();
  initVerticalResizeProbe(stored.popupHeight);

  if (stored.defaultQueueId) {
    defaultQueueSelect.value = stored.defaultQueueId;
  }

  await refreshBridgeStatus();
}

async function expandHlsInBackground(items) {
  if (!bridgeConnected) {
    return;
  }

  const hlsItems = items.filter((item) => item.kind === "hls");
  if (hlsItems.length === 0) {
    return;
  }

  const generation = ++hlsExpandGeneration;
  const expanded = await api.runtime.sendMessage({
    type: "avar-expand-hls-items",
    items,
    referer: pageReferer,
  });

  if (generation !== hlsExpandGeneration) {
    return;
  }

  if (expanded?.ok && Array.isArray(expanded.items)) {
    renderMediaList(expanded.items);
  }
}

async function scanPage() {
  setStatus("Scanning…");
  mediaList.innerHTML = "";
  selectedLinksList.innerHTML = "";
  selectedSection.setAttribute("hidden", "");
  downloadSelectedBtn.hidden = true;
  downloadAllBtn.hidden = true;
  pageReferer = null;
  pageTitle = "";
  hlsExpandGeneration += 1;
  lastSelectedItems = [];

  const response = await api.runtime.sendMessage({ type: "avar-list-media" });
  if (!response?.ok) {
    setStatus(response?.error || "Scan failed.");
    renderSelectedLinksList([]);
    renderMediaList([]);
    return;
  }

  pageReferer = response.pageUrl || null;
  pageTitle = response.pageTitle || "";
  const selectedItems = response.selectedItems || [];
  const items = response.items || (response.urls || []).map((url) => ({
    url,
    kind: AvarMedia.classifyStreamKind(url),
  }));

  renderSelectedLinksList(selectedItems);
  renderMediaList(items);

  const hlsCount = items.filter((item) => item.kind === "hls").length;
  if (hlsCount > 0 && bridgeConnected) {
    setStatus(`${items.length} media URL(s) found. Resolving ${hlsCount} HLS stream(s)…`);
    void expandHlsInBackground(items);
  }
}

settingsBtn.addEventListener("click", () => {
  if (isSettingsOpen()) {
    showMainView();
    return;
  }
  showSettingsView();
});

settingsBackBtn.addEventListener("click", () => {
  showMainView();
});

refreshBtn.addEventListener("click", () => {
  void scanPage();
});

mediaTypeFilterSelect.addEventListener("change", () => {
  renderMediaList(lastMediaItems);
});

mediaSortSelect.addEventListener("change", () => {
  void api.storage.local.set({ mediaSort: getActiveMediaSort() });
  renderMediaList(lastMediaItems);
});

document.getElementById("save").addEventListener("click", async () => {
  const bridgeUrl = AvarExtensionProtocol.normalizeBridgeUrl(
    bridgeUrlInput.value.trim() || DEFAULT_BRIDGE,
  );
  bridgeUrlInput.value = bridgeUrl;
  const defaultMediaFilter = defaultMediaFilterSelect.value || DEFAULT_MEDIA_FILTER;
  await api.runtime.sendMessage({
    type: "avar-set-config",
    bridgeUrl,
    defaultQueueId: defaultQueueSelect.value,
    defaultMediaFilter,
    showSelectionWidget: showSelectionWidgetInput.checked,
  });
  mediaTypeFilterSelect.value = defaultMediaFilter;
  renderMediaList(lastMediaItems);
  setStatus("Settings saved.");
  await refreshBridgeStatus();
  await loadQueues();
});

downloadAllBtn.addEventListener("click", async () => {
  const selectedUrls = new Set(lastSelectedItems.map((item) => item.url));
  const items = getVisibleMediaItems(lastMediaItems).filter((item) => !selectedUrls.has(item.url));
  await openDownloadDialog(items);
});

downloadSelectedBtn.addEventListener("click", async () => {
  await openDownloadDialog(lastSelectedItems);
});

void (async () => {
  await loadConfig();
  void loadQueues();
  installPopupResizeHandles();
  await scanPage();
})();

setInterval(() => void refreshBridgeStatus(), 5000);
