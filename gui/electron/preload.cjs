const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("avar", {
  isElectron: true,
  openPopup: (options) => ipcRenderer.invoke("popup:open", options),
});
