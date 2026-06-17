import { useTranslation } from "react-i18next";
import { useDataStore } from "@/stores/dataStore";

function formatUptime(seconds: number): string {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;

  if (h > 0) {
    return `${h}h ${m}m`;
  }
  if (m > 0) {
    return `${m}m ${s}s`;
  }
  return `${s}s`;
}

export function Footer() {
  const { t } = useTranslation();
  const health = useDataStore((s) => s.health);

  return (
    <footer className="avar-footer">
      {health ? (
        <div className="avar-footer__stats">
          <div className="avar-footer__stat">
            <span className="avar-footer__stat-label">{t("health.uptime")}</span>
            <strong>{formatUptime(health.uptimeSeconds)}</strong>
          </div>
          <div className="avar-footer__stat">
            <span className="avar-footer__stat-label">{t("health.active")}</span>
            <strong>{health.activeDownloads}</strong>
          </div>
          <div className="avar-footer__stat">
            <span className="avar-footer__stat-label">{t("health.queues")}</span>
            <strong>{health.queueCount}</strong>
          </div>
        </div>
      ) : (
        <span className="avar-footer__placeholder">{t("health.unavailable")}</span>
      )}
    </footer>
  );
}
