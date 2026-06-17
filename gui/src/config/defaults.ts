import {
  defaultShortcutMap,
  type ShortcutActionId,
} from "@/shortcuts/definitions";

export type ThemeId = "light" | "dark" | "system";

export type LocaleId = "en" | "fa";

export type SyncChannelId = "poll" | "sse" | "websocket";

export type DetailPanelMode = "pinned" | "inline";

export interface FooterMonitorSettings {
  disk: boolean;
  memory: boolean;
  cpu: boolean;
  network: boolean;
}

export interface GuiSession {
  id: string;
  label: string;
  baseUrl: string;
  authToken?: string;
  useRelativeApi?: boolean;
  useElectronProxy?: boolean;
  /** Built-in sessions cannot be removed (e.g. Electron tunnel). */
  builtin?: boolean;
}

export interface GuiConfig {
  theme: ThemeId;
  locale: LocaleId;
  refreshIntervalMs: number;
  pingIntervalMs: number;
  syncChannel: SyncChannelId;
  activeSessionId: string | null;
  sessions: GuiSession[];
  detailPanelMode: DetailPanelMode;
  shortcuts: Record<ShortcutActionId, string>;
  footerMonitors: FooterMonitorSettings;
  downloadPageSize: number;
  showDownloadCheckboxes: boolean;
}

export const GUI_CONFIG_KEY = "avar.gui.config";
export const GUI_SECRETS_KEY = "avar.gui.secrets";
export const GUI_CONFIG_VERSION = 5;

export const defaultFooterMonitors = (): FooterMonitorSettings => ({
  disk: true,
  memory: true,
  cpu: false,
  network: true,
});

export const ELECTRON_SESSION_ID = "electron-tunnel";

export function createElectronSession(proxyBaseUrl: string): GuiSession {
  return {
    id: ELECTRON_SESSION_ID,
    label: "Electron",
    baseUrl: proxyBaseUrl,
    useElectronProxy: true,
    builtin: true,
  };
}

export const defaultGuiConfig = (): GuiConfig => ({
  theme: "system",
  locale: "en",
  refreshIntervalMs: 3000,
  pingIntervalMs: 2000,
  syncChannel: "poll",
  activeSessionId: "default",
  sessions: [
    {
      id: "default",
      label: "Local daemon",
      baseUrl: "http://127.0.0.1:8000",
      useRelativeApi: false,
    },
  ],
  detailPanelMode: "pinned",
  shortcuts: defaultShortcutMap(),
  footerMonitors: defaultFooterMonitors(),
  downloadPageSize: 100,
  showDownloadCheckboxes: false,
});
