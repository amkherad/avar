const DEFAULT_BRIDGE = "http://127.0.0.1:18766";
const api = typeof browser !== "undefined" ? browser : chrome;

const bridgeUrlInput = document.getElementById("bridgeUrl");
const statusEl = document.getElementById("status");
const bridgeStatusEl = document.getElementById("bridgeStatus");
const mediaList = document.getElementById("mediaList");
const downloadAllBtn = document.getElementById("downloadAll");
const settingsPanel = document.getElementById("settingsPanel");
const settingsBtn = document.getElementById("settingsBtn");

let lastMediaItems = [];

const DOWNLOAD_ICON =
  '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 20h14v-2H5v2Zm7-18-5.5 5.5 1.42 1.42L11 6.17V16h2V6.17l3.08 3.09 1.42-1.42L12 2Z"/></svg>';

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

function renderMediaList(items) {
  lastMediaItems = items;
  mediaList.innerHTML = "";

  for (const item of items) {
    const li = document.createElement("li");
    li.className = "media-item";

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

    const urlLine = document.createElement("div");
    urlLine.className = "media-item__url";
    urlLine.textContent = AvarMedia.formatDisplayUrl(item.url);
    urlLine.title = item.url;

    info.appendChild(name);
    info.appendChild(urlLine);

    const actions = document.createElement("div");
    actions.className = "media-item__actions";

    const sep = document.createElement("span");
    sep.className = "media-item__sep";
    sep.setAttribute("aria-hidden", "true");

    actions.appendChild(sep);
    actions.appendChild(createDownloadButton(item));

    li.appendChild(info);
    li.appendChild(actions);
    mediaList.appendChild(li);
  }

  downloadAllBtn.hidden = items.length === 0;
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
