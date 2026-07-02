import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faCheck, faLightbulb, faMoon } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import type { ThemeId } from "@/config/defaults";
import { useConfigStore } from "@/stores/configStore";
import { useTheme } from "@/theme/ThemeContext";

const HOLD_MS = 450;

const THEME_OPTIONS: { id: ThemeId; labelKey: string }[] = [
  { id: "light", labelKey: "settings.themeLight" },
  { id: "light-bright", labelKey: "settings.themeLightBright" },
  { id: "queen-mode", labelKey: "settings.themeQueenMode" },
  { id: "dark", labelKey: "settings.themeDark" },
  { id: "system", labelKey: "settings.themeSystem" },
];

export function ThemeToggle() {
  const { t } = useTranslation();
  const themeSetting = useConfigStore((s) => s.config.theme);
  const { resolvedMode } = useTheme();
  const updateConfig = useConfigStore((s) => s.updateConfig);

  const [menuOpen, setMenuOpen] = useState(false);
  const containerRef = useRef<HTMLDivElement>(null);
  const holdTimerRef = useRef<number | null>(null);
  const holdOpenedRef = useRef(false);
  const suppressClickRef = useRef(false);

  useEffect(() => {
    if (!menuOpen) {
      return;
    }

    function handleClickOutside(event: MouseEvent) {
      if (containerRef.current && !containerRef.current.contains(event.target as Node)) {
        setMenuOpen(false);
      }
    }

    function handleEscape(event: KeyboardEvent) {
      if (event.key === "Escape") {
        setMenuOpen(false);
      }
    }

    document.addEventListener("mousedown", handleClickOutside);
    document.addEventListener("keydown", handleEscape);
    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
      document.removeEventListener("keydown", handleEscape);
    };
  }, [menuOpen]);

  function clearHoldTimer() {
    if (holdTimerRef.current !== null) {
      window.clearTimeout(holdTimerRef.current);
      holdTimerRef.current = null;
    }
  }

  function openMenuFromHold() {
    holdOpenedRef.current = true;
    suppressClickRef.current = true;
    setMenuOpen(true);
  }

  function handlePointerDown() {
    clearHoldTimer();
    holdOpenedRef.current = false;
    holdTimerRef.current = window.setTimeout(() => {
      holdTimerRef.current = null;
      openMenuFromHold();
    }, HOLD_MS);
  }

  function handlePointerUp() {
    clearHoldTimer();
  }

  function toggle() {
    const next = resolvedMode === "dark" ? "light" : "dark";
    updateConfig({ theme: next });
  }

  function handleClick() {
    if (suppressClickRef.current || holdOpenedRef.current) {
      suppressClickRef.current = false;
      holdOpenedRef.current = false;
      return;
    }
    toggle();
  }

  function selectTheme(theme: ThemeId) {
    updateConfig({ theme });
    setMenuOpen(false);
  }

  const isDark = resolvedMode === "dark";

  return (
    <div className="avar-theme-panel" ref={containerRef}>
      <Button
        variant="ghost"
        size="sm"
        className="avar-header__theme-toggle"
        aria-label={isDark ? t("nav.themeLight") : t("nav.themeDark")}
        aria-expanded={menuOpen}
        aria-haspopup="menu"
        onPointerDown={handlePointerDown}
        onPointerUp={handlePointerUp}
        onPointerLeave={handlePointerUp}
        onPointerCancel={handlePointerUp}
        onClick={handleClick}
      >
        <FontAwesomeIcon icon={isDark ? faLightbulb : faMoon} />
      </Button>

      {menuOpen ? (
        <div
          className="avar-theme-panel__menu"
          role="menu"
          aria-label={t("nav.themeMenu")}
        >
          <ul className="avar-theme-panel__list">
            {THEME_OPTIONS.map((option) => (
              <li key={option.id}>
                <button
                  type="button"
                  role="menuitemradio"
                  aria-checked={themeSetting === option.id}
                  className={`avar-theme-panel__item${themeSetting === option.id ? " avar-theme-panel__item--active" : ""}`}
                  onClick={() => selectTheme(option.id)}
                >
                  <span>{t(option.labelKey)}</span>
                  {themeSetting === option.id ? (
                    <FontAwesomeIcon icon={faCheck} className="avar-theme-panel__check" />
                  ) : null}
                </button>
              </li>
            ))}
          </ul>
        </div>
      ) : null}
    </div>
  );
}
