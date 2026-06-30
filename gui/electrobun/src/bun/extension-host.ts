import type { Server } from "node:http";
import { loadExtensionBridge } from "./vendor-paths";

let extensionBridgeServer: Server | null = null;

export function startExtensionBridge(
  onBatchPopup: (batchId: string, title: string) => void,
  onAddDownloadPopup: (addId: string, title: string) => void,
  onFocusApp: () => void,
): void {
  if (extensionBridgeServer) {
    return;
  }

  const bridge = loadExtensionBridge();
  bridge.setBatchPopupOpener(onBatchPopup);
  bridge.setAddDownloadPopupOpener(onAddDownloadPopup);
  bridge.setAppFocusHandler(onFocusApp);
  extensionBridgeServer = bridge.createExtensionBridgeServer();
  extensionBridgeServer.listen(bridge.EXTENSION_BRIDGE_PORT, bridge.EXTENSION_BRIDGE_HOST);
}

export function stopExtensionBridge(): void {
  if (!extensionBridgeServer) {
    return;
  }
  extensionBridgeServer.close();
  extensionBridgeServer = null;
}

export function getExtensionBridgeModule() {
  return loadExtensionBridge();
}
