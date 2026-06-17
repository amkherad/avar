import { useTranslation } from "react-i18next";
import { QueuePanel } from "@/components/queue/QueuePanel";

export function QueuesPage() {
  const { t } = useTranslation();

  return (
    <div className="avar-queues-page">
      <header className="avar-queues-page__header">
        <h2 className="avar-queues-page__title">{t("queue.manageTitle")}</h2>
        <p className="avar-queues-page__hint">{t("queue.manageHint")}</p>
      </header>
      <div className="avar-queues-page__content">
        <QueuePanel mode="manage" />
      </div>
    </div>
  );
}
