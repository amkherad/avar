import { useTranslation } from "react-i18next";
import type { SettingsCategory } from "@/pages/SettingsPage";

const CATEGORIES: SettingsCategory[] = ["general", "shortcuts"];

export interface SettingsSidebarNavProps {
  category: SettingsCategory;
  onCategoryChange: (category: SettingsCategory) => void;
}

export function SettingsSidebarNav({
  category,
  onCategoryChange,
}: SettingsSidebarNavProps) {
  const { t } = useTranslation();

  return (
    <>
      <h2 className="avar-sidebar-nav__title">{t("settings.title")}</h2>
      <nav className="avar-sidebar-nav avar-sidebar-nav--striped">
        {CATEGORIES.map((id) => (
          <button
            key={id}
            type="button"
            className={`avar-sidebar-nav__item ${category === id ? "avar-sidebar-nav__item--active" : ""}`}
            onClick={() => onCategoryChange(id)}
          >
            {t(`settings.categories.${id}`)}
          </button>
        ))}
      </nav>
    </>
  );
}
