import { create } from "zustand";
import { createJSONStorage, persist } from "zustand/middleware";
import { appLogger } from "@/lib/appLogger";
import {
  defaultFooterMonitors,
  defaultGuiConfig,
  GUI_CONFIG_KEY,
  GUI_CONFIG_VERSION,
  GUI_SECRETS_KEY,
  type GuiConfig,
  type GuiSession,
} from "@/config/defaults";

interface ConfigState {
  config: GuiConfig;
  sessionSecrets: Record<string, string>;
  updateConfig: (patch: Partial<GuiConfig>) => void;
  setConfig: (config: GuiConfig) => void;
  setSessionAuthToken: (sessionId: string, token: string | undefined) => void;
  getSessionWithSecrets: (session: GuiSession) => GuiSession;
}

function mergeConfig(partial: Partial<GuiConfig>): GuiConfig {
  const defaults = defaultGuiConfig();
  return {
    ...defaults,
    ...partial,
    sessions: partial.sessions ?? defaults.sessions,
    shortcuts: { ...defaults.shortcuts, ...partial.shortcuts },
    footerMonitors: { ...defaultFooterMonitors(), ...partial.footerMonitors },
  };
}

function stripSecrets(config: GuiConfig): GuiConfig {
  return {
    ...config,
    sessions: config.sessions.map(({ authToken: _authToken, ...session }) => session),
  };
}

let configHydrated = false;
const configHydrationWaiters: Array<() => void> = [];

function markConfigHydrated(): void {
  if (configHydrated) {
    return;
  }
  configHydrated = true;
  for (const resolve of configHydrationWaiters) {
    resolve();
  }
  configHydrationWaiters.length = 0;
}

/** Wait until GUI config has been restored from localStorage. */
export function waitForConfigHydration(): Promise<void> {
  if (configHydrated) {
    return Promise.resolve();
  }
  return new Promise((resolve) => {
    configHydrationWaiters.push(resolve);
  });
}

export const useConfigStore = create<ConfigState>()(
  persist(
    (set, get) => ({
      config: defaultGuiConfig(),
      sessionSecrets: {},

      updateConfig: (patch) => {
        const keys = Object.keys(patch);
        if (keys.length > 0) {
          appLogger.gui.debug("Config updated", keys.join(", "));
        }
        set((state) => ({
          config: mergeConfig({ ...state.config, ...patch }),
        }));
      },

      setConfig: (config) => {
        appLogger.gui.debug("Config replaced", config.activeSessionId);
        set({ config: mergeConfig(config) });
      },

      setSessionAuthToken: (sessionId, token) => {
        appLogger.gui.debug(
          token?.trim() ? "Session auth token set" : "Session auth token cleared",
          sessionId,
        );
        set((state) => {
          const next = { ...state.sessionSecrets };
          if (token?.trim()) {
            next[sessionId] = token.trim();
          } else {
            delete next[sessionId];
          }
          return { sessionSecrets: next };
        });
      },

      getSessionWithSecrets: (session) => {
        const token = get().sessionSecrets[session.id];
        return token ? { ...session, authToken: token } : session;
      },
    }),
    {
      name: GUI_CONFIG_KEY,
      version: GUI_CONFIG_VERSION,
      partialize: (state) => ({
        config: stripSecrets(state.config),
        sessionSecrets: state.sessionSecrets,
        version: GUI_CONFIG_VERSION,
      }),
      merge: (persisted, current) => {
        const stored = persisted as Partial<{
          config: Partial<GuiConfig>;
          sessionSecrets: Record<string, string>;
        }> | undefined;
        const next =
          !stored?.config
            ? current
            : {
                ...current,
                config: mergeConfig(stored.config),
                sessionSecrets: stored.sessionSecrets ?? {},
              };
        markConfigHydrated();
        return next;
      },
      onRehydrateStorage: () => (_state, error) => {
        if (error) {
          appLogger.gui.error("Config rehydration failed", String(error));
        }
        markConfigHydrated();
      },
      storage: createJSONStorage(() => ({
        getItem: (name) => {
          const raw = localStorage.getItem(name);
          if (!raw && name === GUI_CONFIG_KEY) {
            const legacySecrets = localStorage.getItem(GUI_SECRETS_KEY);
            if (legacySecrets) {
              return JSON.stringify({
                state: {
                  config: defaultGuiConfig(),
                  sessionSecrets: JSON.parse(legacySecrets) as Record<string, string>,
                },
                version: GUI_CONFIG_VERSION,
              });
            }
          }
          return raw;
        },
        setItem: (name, value) => {
          localStorage.setItem(name, value);
        },
        removeItem: (name) => {
          localStorage.removeItem(name);
          localStorage.removeItem(GUI_SECRETS_KEY);
        },
      })),
    },
  ),
);

/** @deprecated Prefer `useConfigStore` selectors for fewer rerenders. */
export function useGuiConfig() {
  const config = useConfigStore((s) => s.config);
  const updateConfig = useConfigStore((s) => s.updateConfig);
  const setConfig = useConfigStore((s) => s.setConfig);
  return { config, updateConfig, setConfig };
}
