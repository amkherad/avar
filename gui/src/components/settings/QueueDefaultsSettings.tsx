import { useTranslation } from "react-i18next";
import { QueuePanel } from "@/components/queue/QueuePanel";

export function QueueDefaultsSettings() {
  const { t } = useTranslation();

  return (
    <div className="avar-settings-form">
      <p className="avar-settings-hint">{t("settings.queues.hint")}</p>
      <QueuePanel mode="manage" />
    </div>
  );
}
