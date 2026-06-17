import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faTableColumns, faTerminal } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { useDataStore } from "@/stores/dataStore";
import { useConsoleStore } from "@/stores/consoleStore";
import { useLayoutStore } from "@/stores/layoutStore";
import { appLogger } from "@/lib/appLogger";

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
  const consoleOpen = useConsoleStore((s) => s.open);
  const hasUnseenErrors = useConsoleStore((s) => s.hasUnseenErrors);
  const toggleConsole = useConsoleStore((s) => s.toggleOpen);
  const detailPanelOpen = useLayoutStore((s) => s.detailPanelOpen);
  const toggleDetailPanel = useLayoutStore((s) => s.toggleDetailPanel);

  return (
    <footer className="avar-footer">
      <div className="avar-footer__main">
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
      </div>

      <div className="avar-footer__toggles">
        <Button
          size="sm"
          variant={detailPanelOpen ? "secondary" : "ghost"}
          className="avar-footer__panel-toggle"
          aria-label={t("download.toggleDetailPanel")}
          aria-pressed={detailPanelOpen}
          onClick={() => {
            appLogger.gui.debug("Detail panel toggled");
            toggleDetailPanel();
          }}
        >
          <FontAwesomeIcon icon={faTableColumns} />
          <span>{t("download.detailPanel")}</span>
        </Button>
        <Button
          size="sm"
          variant={
            consoleOpen ? "secondary" : hasUnseenErrors ? "danger" : "ghost"
          }
          className="avar-footer__console-toggle"
          aria-label={t("console.toggle")}
          aria-pressed={consoleOpen}
          onClick={() => {
            appLogger.gui.debug("Console toggled");
            toggleConsole();
          }}
        >
          <FontAwesomeIcon icon={faTerminal} />
          <span>{t("console.toggle")}</span>
        </Button>
      </div>
    </footer>
  );
}
