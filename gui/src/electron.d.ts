export interface AvarPopupOptions {
  url: string;
  title?: string;
  width?: number;
  height?: number;
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

export interface AvarElectronApi {
  isElectron: true;
  openPopup: (options: AvarPopupOptions) => Promise<number | null>;
  showNotification: (options: AvarNotificationOptions) => Promise<boolean>;
  getProxyBaseUrl: () => Promise<string>;
  setExtensionBridgeEnabled: (enabled: boolean) => Promise<void>;
  setExtensionBridgeConfig: (config: AvarExtensionBridgeConfig) => Promise<void>;
  getExtensionBridgeState: () => Promise<AvarExtensionBridgeState>;
  getExtensionBridgeUrl: () => Promise<string>;
  setTrayLabels: (labels: AvarTrayLabels) => Promise<void>;
}

declare global {
  interface Window {
    avar?: AvarElectronApi;
  }
}

export {};
