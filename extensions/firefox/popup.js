const DEFAULT_BRIDGE = "http://127.0.0.1:18766";
const api = typeof browser !== "undefined" ? browser : chrome;

const bridgeUrlInput = document.getElementById("bridgeUrl");
const statusEl = document.getElementById("status");
const bridgeStatusEl = document.getElementById("bridgeStatus");
const mediaList = document.getElementById("mediaList");
const downloadAllBtn = document.getElementById("downloadAll");
const settingsPanel = document.getElementById("settingsPanel");
const settingsBtn = document.getElementById("settingsBtn");
const refreshBtn = document.getElementById("refreshBtn");

let lastMediaItems = [];

const DOWNLOAD_ICON =
  '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 20h14v-2H5v2zm7-18v12h2V4.5l3.08 3.09 1.42-1.42L12 2 6.5 7.5 7.92 8.92 11 5.83V16h2V5.83l3.08 3.09 1.42-1.42L12 2z"/></svg>';

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
  bridgeStatusEl.title = connected
    ? "Connected to Avar bridge"
    : "Cannot reach Avar bridge";
}

function createDownloadButton(item) {
  const btn = document.createElement("button");
  btn.type = "button";
  btn.className = "media-item__download";
  btn.setAttribute("aria-label", "Download");
  btn.title = "Download";
  btn.innerHTML = DOWNLOAD_ICON;
  btn.addEventListener("click", async () => {
    const result = await api.runtime.sendMessage({
      type: "avar-add-download",
      url: item.url,
      streamKind: item.kind,
    });
    const name = AvarMedia.guessFilename(item.url);
    setStatus(result?.ok ? `Queued: ${name}` : result?.error || "Failed");
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

function renderMediaList(items) {
  lastMediaItems = AvarMedia.sortMediaItems(items);
  mediaList.innerHTML = "";

  for (const item of lastMediaItems) {
    const li = document.createElement("li");
    li.className = "media-item";

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
    const filename = AvarMedia.guessFilename(item.url);
    name.textContent = filename;
    name.title = filename;
    if (item.kind === "hls" || item.kind === "dash") {
      const badge = document.createElement("span");
      badge.className = "media-kind";
      badge.textContent = item.kind;
      name.appendChild(badge);
    }

    const urlRow = document.createElement("div");
    urlRow.className = "media-item__url-row";
    urlRow.appendChild(createCopyButton(item.url));

    const urlLine = document.createElement("span");
    urlLine.className = "media-item__url";
    urlLine.textContent = AvarMedia.formatDisplayUrl(item.url);
    urlLine.title = item.url;
    urlRow.appendChild(urlLine);

    info.appendChild(name);
    info.appendChild(urlRow);

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
    mediaList.appendChild(li);
  }

  downloadAllBtn.hidden = lastMediaItems.length === 0;
}

async function refreshBridgeStatus() {
  const response = await api.runtime.sendMessage({ type: "avar-ping-bridge" });
  setBridgeConnected(Boolean(response?.ok));
  if (response?.bridgeUrl && bridgeUrlInput.value !== response.bridgeUrl) {
    bridgeUrlInput.value = response.bridgeUrl;
  }
  return response;
}

async function loadConfig() {
  const stored = await api.storage.local.get(["bridgeUrl", "guiUrl", "daemonUrl"]);
  bridgeUrlInput.value = stored.bridgeUrl || stored.guiUrl || stored.daemonUrl || DEFAULT_BRIDGE;
  await refreshBridgeStatus();
}

async function scanPage() {
  setStatus("Scanning…");
  mediaList.innerHTML = "";
  downloadAllBtn.hidden = true;

  const response = await api.runtime.sendMessage({ type: "avar-list-media" });
  if (!response?.ok) {
    setStatus(response?.error || "Scan failed.");
    renderMediaList([]);
    return;
  }

  const items = response.items || (response.urls || []).map((url) => ({
    url,
    kind: AvarMedia.classifyStreamKind(url),
  }));
  setStatus(`${items.length} media URL(s) found.`);
  renderMediaList(items);
}

settingsBtn.addEventListener("click", () => {
  const open = settingsPanel.hasAttribute("hidden");
  if (open) {
    settingsPanel.removeAttribute("hidden");
  } else {
    settingsPanel.setAttribute("hidden", "");
  }
});

refreshBtn.addEventListener("click", () => {
  void scanPage();
});

document.getElementById("save").addEventListener("click", async () => {
  const bridgeUrl = bridgeUrlInput.value.trim() || DEFAULT_BRIDGE;
  await api.runtime.sendMessage({
    type: "avar-set-config",
    bridgeUrl,
  });
  setStatus("Settings saved.");
  await refreshBridgeStatus();
});

downloadAllBtn.addEventListener("click", async () => {
  if (lastMediaItems.length === 0) {
    return;
  }

  let count = 0;
  for (const item of lastMediaItems) {
    const result = await api.runtime.sendMessage({
      type: "avar-add-download",
      url: item.url,
      streamKind: item.kind,
    });
    if (result?.ok) {
      count += 1;
    }
  }
  setStatus(`Queued ${count} download(s).`);
});

void (async () => {
  await loadConfig();
  await scanPage();
})();

setInterval(() => void refreshBridgeStatus(), 5000);
