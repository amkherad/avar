import { useTranslation } from "react-i18next";
import { GeneralSettings } from "@/components/settings/GeneralSettings";
import { ShortcutsSettings } from "@/components/settings/ShortcutsSettings";

export type SettingsCategory = "general" | "shortcuts";

export interface SettingsPageProps {
  category: SettingsCategory;
}

export function SettingsPage({ category }: SettingsPageProps) {
  const { t } = useTranslation();

  return (
    <div className="avar-settings-page__content">
      <h2 className="avar-settings-page__heading">{t(`settings.categories.${category}`)}</h2>
      {category === "general" ? <GeneralSettings /> : <ShortcutsSettings />}
    </div>
  );
}
