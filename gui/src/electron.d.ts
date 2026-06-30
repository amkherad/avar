export interface AvarPopupOptions {
  /** Popup route hash (preferred in Electron — main process resolves the app URL). */
  hash?: string;
  /** Full popup URL (web in-app popups; legacy Electron fallback). */
  url?: string;
  title?: string;
  width?: number;
  height?: number;
  minWidth?: number;
  minHeight?: number;
}

export interface AvarNotificationOptions {
  title: string;
  body?: string;
}

export interface AvarExtensionBridgeConfig {
  daemonUrl: string;
  authToken?: string;
}

export interface AvarTrayLabels {
  show: string;
  exit: string;
  startAll: string;
  pauseAll: string;
  resumeAll: string;
  stopAll: string;
}

export interface AvarTrayActiveDownload {
  id: string;
  filename: string;
  percent: number;
}

export interface AvarTrayActiveDownloads {
  sectionLabel: string;
  items: AvarTrayActiveDownload[];
}

export interface AvarExtensionBridgeState {
  enabled: boolean;
  connected: boolean;
  lastPingAt: number | null;
  lastExtensionVersion: string | null;
  bridgeVersion: string;
  protocolVersion: number;
  port: number;
  host: string;
}

export interface AvarSaveRemoteDownloadFileOptions {
  fileUrl: string;
  authToken?: string;
  destDir: string;
  filename: string;
}

export interface AvarSelectDirectoryOptions {
  defaultPath?: string;
  title?: string;
}

export interface AvarElectronApi {
  isElectron: true;
  openPopup: (options: AvarPopupOptions) => Promise<number | null>;
  selectDirectory: (options?: AvarSelectDirectoryOptions) => Promise<string | null>;
  showNotification: (options: AvarNotificationOptions) => Promise<boolean>;
  getProxyBaseUrl: () => Promise<string>;
  saveRemoteDownloadFile: (options: AvarSaveRemoteDownloadFileOptions) => Promise<void>;
  setExtensionBridgeEnabled: (enabled: boolean) => Promise<void>;
  setExtensionBridgeConfig: (config: AvarExtensionBridgeConfig) => Promise<void>;
  getExtensionBridgeState: () => Promise<AvarExtensionBridgeState>;
  getExtensionBridgeUrl: () => Promise<string>;
  setTrayLabels: (labels: AvarTrayLabels) => Promise<void>;
  setTrayActiveDownloads: (payload: AvarTrayActiveDownloads) => Promise<void>;
  openPath: (filePath: string) => Promise<string>;
  showItemInFolder: (filePath: string) => Promise<string>;
  openExternal: (url: string) => Promise<boolean>;
}

declare global {
  interface Window {
    avar?: AvarElectronApi;
  }
}

export {};
