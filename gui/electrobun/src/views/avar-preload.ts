/**
 * Electrobun preload — exposes the same `window.avar` API as Electron preload.
 */
import { Electroview } from "electrobun/view";
import type { AvarRpcSchema } from "../../shared/avar-rpc";

declare global {
  interface Window {
    avar?: {
      isElectron: true;
      openPopup: (options: Record<string, unknown>) => Promise<number | null>;
      selectDirectory: (options?: Record<string, unknown>) => Promise<string | null>;
      showNotification: (options?: Record<string, unknown>) => Promise<boolean>;
      getProxyBaseUrl: () => Promise<string>;
      saveRemoteDownloadFile: (options: Record<string, unknown>) => Promise<void>;
      setExtensionBridgeEnabled: (enabled: boolean) => Promise<void>;
      setExtensionBridgeConfig: (config: Record<string, unknown>) => Promise<void>;
      getExtensionBridgeState: () => Promise<Record<string, unknown>>;
      getExtensionBridgeUrl: () => Promise<string>;
      setTrayLabels: (labels: Record<string, unknown>) => Promise<void>;
      setTrayActiveDownloads: (payload: Record<string, unknown>) => Promise<void>;
      openPath: (filePath: string) => Promise<string>;
      showItemInFolder: (filePath: string) => Promise<string>;
      openExternal: (url: string) => Promise<boolean>;
      setKeepInTrayOnClose: (enabled: boolean) => Promise<void>;
    };
  }
}

const rpc = Electroview.defineRPC<AvarRpcSchema>({
  handlers: {
    requests: {},
  },
});

const electroview = new Electroview({ rpc });

window.avar = {
  isElectron: true,
  openPopup: (options) => rpc.request.openPopup(options),
  selectDirectory: (options) => rpc.request.selectDirectory(options),
  showNotification: (options) => rpc.request.showNotification(options),
  getProxyBaseUrl: () => rpc.request.getProxyBaseUrl(),
  saveRemoteDownloadFile: (options) => rpc.request.saveRemoteDownloadFile(options),
  setExtensionBridgeEnabled: (enabled) => rpc.request.setExtensionBridgeEnabled(enabled),
  setExtensionBridgeConfig: (config) => rpc.request.setExtensionBridgeConfig(config),
  getExtensionBridgeState: () => rpc.request.getExtensionBridgeState(),
  getExtensionBridgeUrl: () => rpc.request.getExtensionBridgeUrl(),
  setTrayLabels: (labels) => rpc.request.setTrayLabels(labels),
  setTrayActiveDownloads: (payload) => rpc.request.setTrayActiveDownloads(payload),
  openPath: (filePath) => rpc.request.openPath(filePath),
  showItemInFolder: (filePath) => rpc.request.showItemInFolder(filePath),
  openExternal: (url) => rpc.request.openExternal(url),
  setKeepInTrayOnClose: (enabled) => rpc.request.setKeepInTrayOnClose(enabled),
};

void electroview;
