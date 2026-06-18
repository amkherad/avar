const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("avar", {
  isElectron: true,
  openPopup: (options) => ipcRenderer.invoke("popup:open", options),
  showNotification: (options) => ipcRenderer.invoke("notification:show", options),
  getProxyBaseUrl: () => ipcRenderer.invoke("daemon:getProxyBaseUrl"),
  setExtensionBridgeEnabled: (enabled) =>
    ipcRenderer.invoke("extensionBridge:setEnabled", enabled),
  setExtensionBridgeConfig: (config) =>
    ipcRenderer.invoke("extensionBridge:setConfig", config),
  getExtensionBridgeState: () => ipcRenderer.invoke("extensionBridge:getState"),
  getExtensionBridgeUrl: () => ipcRenderer.invoke("extensionBridge:getUrl"),
  setTrayLabels: (labels) => ipcRenderer.invoke("tray:setLabels", labels),
});
