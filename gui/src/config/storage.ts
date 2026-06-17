import {
  defaultGuiConfig,
  GUI_CONFIG_KEY,
  GUI_CONFIG_VERSION,
  type GuiConfig,
} from "./defaults";

function mergeConfig(partial: Partial<GuiConfig>): GuiConfig {
  const defaults = defaultGuiConfig();
  return {
    ...defaults,
    ...partial,
    sessions: partial.sessions ?? defaults.sessions,
  };
}

export function loadGuiConfig(): GuiConfig {
  try {
    const raw = localStorage.getItem(GUI_CONFIG_KEY);
    if (!raw) {
      return defaultGuiConfig();
    }
    const parsed = JSON.parse(raw) as Partial<GuiConfig> & { version?: number };
    if (parsed.version !== GUI_CONFIG_VERSION) {
      return mergeConfig(parsed);
    }
    return mergeConfig(parsed);
  } catch {
    return defaultGuiConfig();
  }
}

export function saveGuiConfig(config: GuiConfig): void {
  localStorage.setItem(
    GUI_CONFIG_KEY,
    JSON.stringify({ ...config, version: GUI_CONFIG_VERSION }),
  );
}
