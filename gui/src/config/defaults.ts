export type ThemeId = "light" | "dark" | "system";

export type LocaleId = "en" | "fa";

export type SyncChannelId = "poll" | "sse" | "websocket";

export type DetailPanelMode = "pinned" | "inline";

export interface GuiSession {
  id: string;
  label: string;
  baseUrl: string;
  authToken?: string;
  useRelativeApi?: boolean;
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
}

export const GUI_CONFIG_KEY = "avar.gui.config";
export const GUI_CONFIG_VERSION = 3;

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
});
