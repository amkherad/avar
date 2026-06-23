export type ThemeVariantId = "light-soft" | "light-bright" | "dark";

export interface ThemeTokens {
  id: ThemeVariantId;
  bg: string;
  bgElevated: string;
  bgMuted: string;
  border: string;
  text: string;
  textMuted: string;
  primary: string;
  primaryHover: string;
  primaryText: string;
  success: string;
  warning: string;
  danger: string;
  shadow: string;
  radius: string;
  font: string;
}

/** Default light theme — muted surfaces for bright rooms and large displays. */
export const softLightTheme: ThemeTokens = {
  id: "light-soft",
  bg: "#d4d0c8",
  bgElevated: "#e2ded6",
  bgMuted: "#c8c4bc",
  border: "#b4b0a8",
  text: "#2a2824",
  textMuted: "#5c5850",
  primary: "#2563eb",
  primaryHover: "#1d4ed8",
  primaryText: "#f4f2ee",
  success: "#15803d",
  warning: "#b45309",
  danger: "#b91c1c",
  shadow: "0 8px 24px rgba(42, 40, 36, 0.12)",
  radius: "10px",
  font: "'Segoe UI', system-ui, -apple-system, sans-serif",
};

/** High-contrast light theme with brighter surfaces. */
export const brightLightTheme: ThemeTokens = {
  id: "light-bright",
  bg: "#f4f6f8",
  bgElevated: "#ffffff",
  bgMuted: "#e8ecf0",
  border: "#d5dbe3",
  text: "#1a2332",
  textMuted: "#5c6b7f",
  primary: "#2563eb",
  primaryHover: "#1d4ed8",
  primaryText: "#ffffff",
  success: "#16a34a",
  warning: "#d97706",
  danger: "#dc2626",
  shadow: "0 8px 24px rgba(15, 23, 42, 0.08)",
  radius: "10px",
  font: "'Segoe UI', system-ui, -apple-system, sans-serif",
};

export const darkTheme: ThemeTokens = {
  id: "dark",
  bg: "#0f1419",
  bgElevated: "#1a222d",
  bgMuted: "#242d3a",
  border: "#334155",
  text: "#e8edf4",
  textMuted: "#94a3b8",
  primary: "#3b82f6",
  primaryHover: "#60a5fa",
  primaryText: "#0f1419",
  success: "#22c55e",
  warning: "#f59e0b",
  danger: "#ef4444",
  shadow: "0 8px 24px rgba(0, 0, 0, 0.35)",
  radius: "10px",
  font: "'Segoe UI', system-ui, -apple-system, sans-serif",
};

export function resolveSystemTheme(): "light" | "dark" {
  if (typeof window === "undefined") {
    return "light";
  }
  return window.matchMedia("(prefers-color-scheme: dark)").matches
    ? "dark"
    : "light";
}

export function resolveThemeTokens(
  themeSetting: "light" | "light-bright" | "dark" | "system",
  systemMode: "light" | "dark",
): ThemeTokens {
  if (themeSetting === "dark") {
    return darkTheme;
  }
  if (themeSetting === "light-bright") {
    return brightLightTheme;
  }
  if (themeSetting === "system") {
    return systemMode === "dark" ? darkTheme : softLightTheme;
  }
  return softLightTheme;
}

export function themeToCssVars(theme: ThemeTokens): Record<string, string> {
  return {
    "--avar-bg": theme.bg,
    "--avar-bg-elevated": theme.bgElevated,
    "--avar-bg-muted": theme.bgMuted,
    "--avar-border": theme.border,
    "--avar-text": theme.text,
    "--avar-text-muted": theme.textMuted,
    "--avar-primary": theme.primary,
    "--avar-primary-hover": theme.primaryHover,
    "--avar-primary-text": theme.primaryText,
    "--avar-success": theme.success,
    "--avar-warning": theme.warning,
    "--avar-danger": theme.danger,
    "--avar-shadow": theme.shadow,
    "--avar-radius": theme.radius,
    "--avar-font": theme.font,
    "--avar-scrollbar-track": theme.bgMuted,
    "--avar-scrollbar-thumb": theme.border,
    "--avar-scrollbar-thumb-hover": theme.textMuted,
  };
}
