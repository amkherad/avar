import { create } from "zustand";
import { persist } from "zustand/middleware";
import {
  defaultGuiConfig,
  GUI_CONFIG_KEY,
  GUI_CONFIG_VERSION,
  type GuiConfig,
} from "@/config/defaults";

interface ConfigState {
  config: GuiConfig;
  updateConfig: (patch: Partial<GuiConfig>) => void;
  setConfig: (config: GuiConfig) => void;
}

function mergeConfig(partial: Partial<GuiConfig>): GuiConfig {
  const defaults = defaultGuiConfig();
  return {
    ...defaults,
    ...partial,
    sessions: partial.sessions ?? defaults.sessions,
    shortcuts: { ...defaults.shortcuts, ...partial.shortcuts },
  };
}

export const useConfigStore = create<ConfigState>()(
  persist(
    (set) => ({
      config: defaultGuiConfig(),
      updateConfig: (patch) =>
        set((state) => ({ config: { ...state.config, ...patch } })),
      setConfig: (config) => set({ config }),
    }),
    {
      name: GUI_CONFIG_KEY,
      version: GUI_CONFIG_VERSION,
      partialize: (state) => ({
        config: state.config,
        version: GUI_CONFIG_VERSION,
      }),
      merge: (persisted, current) => {
        const stored = persisted as Partial<{ config: Partial<GuiConfig> }> | undefined;
        if (!stored?.config) {
          return current;
        }
        return {
          ...current,
          config: mergeConfig(stored.config),
        };
      },
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
