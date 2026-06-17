import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faTableColumns, faTerminal } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { useDataStore } from "@/stores/dataStore";
import { useConsoleStore } from "@/stores/consoleStore";
import { useLayoutStore } from "@/stores/layoutStore";
import { useConfigStore } from "@/stores/configStore";
import { useConnectionStore } from "@/stores/connectionStore";
import { formatBytes, formatPercent } from "@/lib/formatBytes";
import { appLogger } from "@/lib/appLogger";
import type { SystemStatsInfo } from "@/api/types";

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
  const client = useConnectionStore((s) => s.client);
  const connection = useConnectionStore((s) => s.connection);
  const footerMonitors = useConfigStore((s) => s.config.footerMonitors);
  const consoleOpen = useConsoleStore((s) => s.open);
  const hasUnseenErrors = useConsoleStore((s) => s.hasUnseenErrors);
  const toggleConsole = useConsoleStore((s) => s.toggleOpen);
  const detailPanelOpen = useLayoutStore((s) => s.detailPanelOpen);
  const toggleDetailPanel = useLayoutStore((s) => s.toggleDetailPanel);

  const [stats, setStats] = useState<SystemStatsInfo | null>(null);
  const monitorsEnabled =
    footerMonitors.disk || footerMonitors.memory || footerMonitors.cpu || footerMonitors.network;

  useEffect(() => {
    if (!client || connection !== "connected" || !monitorsEnabled) {
      setStats(null);
      return;
    }

    let cancelled = false;

    async function poll() {
      try {
        const next = await client!.systemStats();
        if (!cancelled) {
          setStats(next);
        }
      } catch {
        if (!cancelled) {
          setStats(null);
        }
      }
    }

    void poll();
    const timer = window.setInterval(() => void poll(), 3000);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [client, connection, monitorsEnabled]);

  const diskUsedPercent =
    stats && stats.diskTotalBytes > 0
      ? ((stats.diskTotalBytes - stats.diskFreeBytes) / stats.diskTotalBytes) * 100
      : 0;

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
            {footerMonitors.disk && stats ? (
              <div className="avar-footer__stat">
                <span className="avar-footer__stat-label">{t("health.disk")}</span>
                <strong>
                  {formatBytes(stats.diskFreeBytes)} / {formatBytes(stats.diskTotalBytes)} (
                  {formatPercent(diskUsedPercent)} {t("health.used")})
                </strong>
              </div>
            ) : null}
            {footerMonitors.memory && stats ? (
              <div className="avar-footer__stat">
                <span className="avar-footer__stat-label">{t("health.memory")}</span>
                <strong>
                  {formatBytes(stats.memoryUsedBytes)} / {formatBytes(stats.memoryTotalBytes)} (
                  {formatPercent(stats.memoryUsedPercent)})
                </strong>
              </div>
            ) : null}
            {footerMonitors.cpu && stats ? (
              <div className="avar-footer__stat">
                <span className="avar-footer__stat-label">{t("health.cpu")}</span>
                <strong>{formatPercent(stats.cpuUsagePercent)}</strong>
              </div>
            ) : null}
            {footerMonitors.network && stats ? (
              <div className="avar-footer__stat">
                <span className="avar-footer__stat-label">{t("health.network")}</span>
                <strong>{formatBytes(stats.networkRxBytesPerSec)}/s</strong>
              </div>
            ) : null}
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
