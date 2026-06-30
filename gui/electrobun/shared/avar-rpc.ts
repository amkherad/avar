/**
 * Typed RPC contract between the Electrobun main process and the Avar GUI webview.
 * Mirrors the Electron preload API (`window.avar` in gui/electron/preload.cjs).
 */

export type AvarPopupOptions = {
  url?: string;
  hash?: string;
  title?: string;
  width?: number;
  height?: number;
  minWidth?: number;
  minHeight?: number;
};

export type AvarSelectDirectoryOptions = {
  defaultPath?: string;
  title?: string;
};

export type AvarNotificationOptions = {
  title?: string;
  body?: string;
  tag?: string;
};

export type AvarSaveRemoteDownloadFileOptions = {
  fileUrl: string;
  destDir: string;
  filename: string;
  authToken?: string;
};

export type AvarExtensionBridgeConfig = {
  daemonUrl: string;
  authToken?: string;
};

export type AvarExtensionBridgeState = {
  enabled: boolean;
  connected: boolean;
  lastPingAt: number | null;
  lastExtensionVersion: string | null;
  bridgeVersion: string;
  protocolVersion: number;
  port: number;
  host: string;
};

export type AvarTrayLabels = {
  show?: string;
  exit?: string;
  startAll?: string;
  pauseAll?: string;
  resumeAll?: string;
  stopAll?: string;
};

export type AvarTrayActiveDownloadItem = {
  id: string;
  filename: string;
  percent: number;
};

export type AvarTrayActiveDownloads = {
  sectionLabel?: string;
  items?: AvarTrayActiveDownloadItem[];
};

export type AvarRpcSchema = {
  bun: {
    requests: {
      openPopup: { params: AvarPopupOptions; response: number | null };
      selectDirectory: { params: AvarSelectDirectoryOptions | undefined; response: string | null };
      showNotification: { params: AvarNotificationOptions; response: boolean };
      getProxyBaseUrl: { params: undefined; response: string };
      saveRemoteDownloadFile: { params: AvarSaveRemoteDownloadFileOptions; response: void };
      setExtensionBridgeEnabled: { params: boolean; response: void };
      setExtensionBridgeConfig: { params: AvarExtensionBridgeConfig; response: void };
      getExtensionBridgeState: { params: undefined; response: AvarExtensionBridgeState };
      getExtensionBridgeUrl: { params: undefined; response: string };
      setTrayLabels: { params: AvarTrayLabels; response: void };
      setTrayActiveDownloads: { params: AvarTrayActiveDownloads; response: void };
      openPath: { params: string; response: string };
      showItemInFolder: { params: string; response: string };
      openExternal: { params: string; response: boolean };
      setKeepInTrayOnClose: { params: boolean; response: void };
    };
    messages: Record<never, unknown>;
  };
  webview: {
    requests: Record<never, unknown>;
    messages: Record<never, unknown>;
  };
};
