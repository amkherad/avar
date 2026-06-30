const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("avar", {
  isElectron: true,
  platform: process.platform,
  minimizeWindow: () => ipcRenderer.invoke("window:minimize"),
  maximizeWindow: () => ipcRenderer.invoke("window:maximize"),
  closeWindow: () => ipcRenderer.invoke("window:close"),
  isWindowMaximized: () => ipcRenderer.invoke("window:isMaximized"),
  openPopup: (options) => ipcRenderer.invoke("popup:open", options),
  selectDirectory: (options) => ipcRenderer.invoke("dialog:selectDirectory", options),
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
  openPath: (filePath) => ipcRenderer.invoke("download:openLocalFile", filePath),
  showItemInFolder: (filePath) => ipcRenderer.invoke("download:showItemInFolder", filePath),
  openExternal: (url) => ipcRenderer.invoke("shell:openExternal", url),
  setKeepInTrayOnClose: (enabled) =>
    ipcRenderer.invoke("desktop:setKeepInTrayOnClose", enabled),
});
