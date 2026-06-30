import {
  defaultShortcutMap,
  type ShortcutActionId,
} from "@/shortcuts/definitions";

export type ThemeId = "light" | "light-bright" | "dark" | "system";

export type LocaleId = "en" | "fa";

export type SyncChannelId = "poll" | "sse" | "websocket";

export type DetailPanelMode = "pinned" | "inline";

export type DownloadDoubleClickAction = "openFile" | "openDetails";

export type ByteDisplayUnit = "binary" | "decimal";

export type TransferRateDisplayUnit = "binary-bytes" | "binary-bits";

export type FooterMonitorDisplay = "text" | "histogram";

export interface FooterMonitorSettings {
  disk: boolean;
  memory: boolean;
  cpu: boolean;
  network: boolean;
  display: FooterMonitorDisplay;
}

export interface GuiSession {
  id: string;
  label: string;
  baseUrl: string;
  authToken?: string;
  useRelativeApi?: boolean;
  useElectronProxy?: boolean;
  /** Override auto-detection and treat this session as remote. */
  forceRemote?: boolean;
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
  browserExtensionEnabled: boolean;
  notificationsEnabled: boolean;
  /** Local folder for copying files from a remote daemon (GUI-only). */
  localDownloadPath: string;
  /** Double-click behavior on download rows (local sessions; open file requires Electron). */
  downloadDoubleClickAction: DownloadDoubleClickAction;
  /** Size labels in download progress and system monitors (binary KiB vs decimal KB). */
  byteDisplayUnit: ByteDisplayUnit;
  /** Transfer rate labels (binary KiB/s vs binary Kib/s). */
  transferRateDisplayUnit: TransferRateDisplayUnit;
  /** When true, closing the main window hides to the system tray (desktop only). */
  keepInTrayOnClose: boolean;
}

export const GUI_CONFIG_KEY = "avar.gui.config";
export const GUI_SECRETS_KEY = "avar.gui.secrets";
export const GUI_CONFIG_VERSION = 12;

export const defaultFooterMonitors = (): FooterMonitorSettings => ({
  disk: true,
  memory: true,
  cpu: false,
  network: true,
  display: "text",
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
  syncChannel: "sse",
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
  showDownloadCheckboxes: true,
  browserExtensionEnabled: true,
  notificationsEnabled: true,
  localDownloadPath: "",
  downloadDoubleClickAction: "openFile",
  byteDisplayUnit: "binary",
  transferRateDisplayUnit: "binary-bytes",
  keepInTrayOnClose: true,
});
