import {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";
import { useConfigStore } from "@/stores/configStore";
import {
  resolveSystemTheme,
  resolveThemeTokens,
  themeToCssVars,
  type ThemeTokens,
} from "./themes";

interface ThemeContextValue {
  theme: ThemeTokens;
  resolvedMode: "light" | "dark";
}

const ThemeContext = createContext<ThemeContextValue | null>(null);

export function ThemeProvider({ children }: { children: ReactNode }) {
  const themeSetting = useConfigStore((s) => s.config.theme);
  const [systemMode, setSystemMode] = useState<"light" | "dark">(resolveSystemTheme);

  useEffect(() => {
    const media = window.matchMedia("(prefers-color-scheme: dark)");
    const handler = () => setSystemMode(resolveSystemTheme());
    media.addEventListener("change", handler);
    return () => media.removeEventListener("change", handler);
  }, []);

  const resolvedMode =
    themeSetting === "system"
      ? systemMode
      : themeSetting === "dark"
        ? "dark"
        : "light";

  const theme = useMemo(
    () => resolveThemeTokens(themeSetting, systemMode),
    [themeSetting, systemMode],
  );
  const cssVars = useMemo(() => themeToCssVars(theme), [theme]);

  useEffect(() => {
    const root = document.documentElement;
    for (const [key, value] of Object.entries(cssVars)) {
      root.style.setProperty(key, value);
    }
    root.dataset.avarTheme = theme.id;
    return () => {
      for (const key of Object.keys(cssVars)) {
        root.style.removeProperty(key);
      }
      delete root.dataset.avarTheme;
    };
  }, [cssVars, theme.id]);

  const value = useMemo(
    () => ({ theme, resolvedMode }),
    [theme, resolvedMode],
  );

  return (
    <ThemeContext.Provider value={value}>
      <div className="avar-root">{children}</div>
    </ThemeContext.Provider>
  );
}

export function useTheme(): ThemeContextValue {
  const ctx = useContext(ThemeContext);
  if (!ctx) {
    throw new Error("useTheme must be used within ThemeProvider");
  }
  return ctx;
}
