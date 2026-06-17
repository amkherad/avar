const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("avar", {
  isElectron: true,
  openPopup: (options) => ipcRenderer.invoke("popup:open", options),
  showNotification: (options) => ipcRenderer.invoke("notification:show", options),
  getProxyBaseUrl: () => ipcRenderer.invoke("daemon:getProxyBaseUrl"),
});
