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

export interface AvarElectronApi {
  isElectron: true;
  openPopup: (options: AvarPopupOptions) => Promise<number | null>;
  showNotification: (options: AvarNotificationOptions) => Promise<boolean>;
  getProxyBaseUrl: () => Promise<string>;
}

declare global {
  interface Window {
    avar?: AvarElectronApi;
  }
}

export {};
