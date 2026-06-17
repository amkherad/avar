import { useState } from "react";
import { useTranslation } from "react-i18next";
import { GeneralSettings } from "@/components/settings/GeneralSettings";
import { ShortcutsSettings } from "@/components/settings/ShortcutsSettings";

export type SettingsCategory = "general" | "shortcuts";

const CATEGORIES: SettingsCategory[] = ["general", "shortcuts"];

export function SettingsPage() {
  const { t } = useTranslation();
  const [category, setCategory] = useState<SettingsCategory>("general");

  return (
    <div className="avar-settings-page">
      <aside className="avar-settings-page__sidebar">
        <h2 className="avar-settings-page__sidebar-title">{t("settings.title")}</h2>
        <nav className="avar-settings-page__nav">
          {CATEGORIES.map((id) => (
            <button
              key={id}
              type="button"
              className={`avar-settings-page__nav-item ${category === id ? "avar-settings-page__nav-item--active" : ""}`}
              onClick={() => setCategory(id)}
            >
              {t(`settings.categories.${id}`)}
            </button>
          ))}
        </nav>
      </aside>

      <div className="avar-settings-page__content">
        <h2 className="avar-settings-page__heading">{t(`settings.categories.${category}`)}</h2>
        {category === "general" ? <GeneralSettings /> : <ShortcutsSettings />}
      </div>
    </div>
  );
}
