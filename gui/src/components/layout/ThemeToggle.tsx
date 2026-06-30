import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faLightbulb, faMoon } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { useConfigStore } from "@/stores/configStore";
import { useTheme } from "@/theme/ThemeContext";

export function ThemeToggle() {
  const { t } = useTranslation();
  const { resolvedMode } = useTheme();
  const updateConfig = useConfigStore((s) => s.updateConfig);

  function toggle() {
    const next = resolvedMode === "dark" ? "light" : "dark";
    updateConfig({ theme: next });
  }

  const isDark = resolvedMode === "dark";

  return (
    <Button
      variant="ghost"
      size="sm"
      className="avar-header__theme-toggle"
      aria-label={isDark ? t("nav.themeLight") : t("nav.themeDark")}
      onClick={toggle}
    >
      <FontAwesomeIcon icon={isDark ? faLightbulb : faMoon} />
    </Button>
  );
}
