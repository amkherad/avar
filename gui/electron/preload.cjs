const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("avar", {
  isElectron: true,
  openPopup: (options) => ipcRenderer.invoke("popup:open", options),
  showNotification: (options) => ipcRenderer.invoke("notification:show", options),
  getProxyBaseUrl: () => ipcRenderer.invoke("daemon:getProxyBaseUrl"),
  saveRemoteDownloadFile: (options) =>
    ipcRenderer.invoke("download:saveRemoteFile", options),
  setExtensionBridgeEnabled: (enabled) =>
    ipcRenderer.invoke("extensionBridge:setEnabled", enabled),
  setExtensionBridgeConfig: (config) =>
    ipcRenderer.invoke("extensionBridge:setConfig", config),
  getExtensionBridgeState: () => ipcRenderer.invoke("extensionBridge:getState"),
  getExtensionBridgeUrl: () => ipcRenderer.invoke("extensionBridge:getUrl"),
  setTrayLabels: (labels) => ipcRenderer.invoke("tray:setLabels", labels),
  setTrayActiveDownloads: (payload) => ipcRenderer.invoke("tray:setActiveDownloads", payload),
});
