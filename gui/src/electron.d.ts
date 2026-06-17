export interface AvarPopupOptions {
  url: string;
  title?: string;
  width?: number;
  height?: number;
}

export interface AvarElectronApi {
  isElectron: true;
  openPopup: (options: AvarPopupOptions) => Promise<number | null>;
}

declare global {
  interface Window {
    avar?: AvarElectronApi;
  }
}

export {};
