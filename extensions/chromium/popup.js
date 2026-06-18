const DEFAULT_BRIDGE = "http://127.0.0.1:18766";

const bridgeUrlInput = document.getElementById("bridgeUrl");
const statusEl = document.getElementById("status");
const bridgeStatusEl = document.getElementById("bridgeStatus");
const mediaList = document.getElementById("mediaList");

function setStatus(text) {
  statusEl.textContent = text;
}

function setBridgeConnected(connected) {
  bridgeStatusEl.classList.toggle("status-dot--ok", connected);
  bridgeStatusEl.title = connected
    ? "Connected to Avar bridge"
    : "Cannot reach Avar bridge";
}

async function refreshBridgeStatus() {
  const response = await chrome.runtime.sendMessage({ type: "avar-ping-bridge" });
  setBridgeConnected(Boolean(response?.ok));
  if (response?.bridgeUrl && bridgeUrlInput.value !== response.bridgeUrl) {
    bridgeUrlInput.value = response.bridgeUrl;
  }
  return response;
}

async function loadConfig() {
  const stored = await chrome.storage.local.get(["bridgeUrl", "guiUrl", "daemonUrl"]);
  bridgeUrlInput.value = stored.bridgeUrl || stored.guiUrl || stored.daemonUrl || DEFAULT_BRIDGE;
  await refreshBridgeStatus();
}

document.getElementById("save").addEventListener("click", async () => {
  const bridgeUrl = bridgeUrlInput.value.trim() || DEFAULT_BRIDGE;
  await chrome.runtime.sendMessage({
    type: "avar-set-config",
    bridgeUrl,
  });
  setStatus("Settings saved.");
  await refreshBridgeStatus();
});

document.getElementById("scan").addEventListener("click", async () => {
  setStatus("Scanning…");
  mediaList.innerHTML = "";

  const response = await chrome.runtime.sendMessage({ type: "avar-list-media" });
  if (!response?.ok) {
    setStatus(response?.error || "Scan failed.");
    return;
  }

  const urls = response.urls || [];
  setStatus(`${urls.length} media URL(s) found.`);

  for (const url of urls) {
    const li = document.createElement("li");
    li.textContent = url;
    const btn = document.createElement("button");
    btn.type = "button";
    btn.textContent = "Download";
    btn.addEventListener("click", async () => {
      const result = await chrome.runtime.sendMessage({ type: "avar-add-download", url });
      setStatus(result?.ok ? `Queued: ${url}` : result?.error || "Failed");
    });
    li.appendChild(btn);
    mediaList.appendChild(li);
  }
});

document.getElementById("downloadAll").addEventListener("click", async () => {
  const response = await chrome.runtime.sendMessage({ type: "avar-list-media" });
  if (!response?.ok) {
    setStatus(response?.error || "Scan failed.");
    return;
  }

  let count = 0;
  for (const url of response.urls || []) {
    const result = await chrome.runtime.sendMessage({ type: "avar-add-download", url });
    if (result?.ok) {
      count += 1;
    }
  }
  setStatus(`Queued ${count} download(s).`);
});

void loadConfig();
setInterval(() => void refreshBridgeStatus(), 5000);
