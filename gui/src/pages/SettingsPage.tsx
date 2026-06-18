import { useTranslation } from "react-i18next";
import { GeneralSettings } from "@/components/settings/GeneralSettings";
import { ShortcutsSettings } from "@/components/settings/ShortcutsSettings";
import { DownloadSettings } from "@/components/settings/DownloadSettings";
import { QueueDefaultsSettings } from "@/components/settings/QueueDefaultsSettings";
import { DaemonSettings } from "@/components/settings/DaemonSettings";
import { BrowserIntegrationSettings } from "@/components/settings/BrowserIntegrationSettings";

export type SettingsCategory = "general" | "downloads" | "queues" | "daemon" | "browser" | "shortcuts";

export interface SettingsPageProps {
  category: SettingsCategory;
}

export function SettingsPage({ category }: SettingsPageProps) {
  const { t } = useTranslation();

  return (
    <div className="avar-settings-page__content">
      <h2 className="avar-settings-page__heading">{t(`settings.categories.${category}`)}</h2>
      {category === "general" ? <GeneralSettings /> : null}
      {category === "downloads" ? <DownloadSettings /> : null}
      {category === "queues" ? <QueueDefaultsSettings /> : null}
      {category === "daemon" ? <DaemonSettings /> : null}
      {category === "browser" ? <BrowserIntegrationSettings /> : null}
      {category === "shortcuts" ? <ShortcutsSettings /> : null}
    </div>
  );
}
