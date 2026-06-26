import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faTableColumns, faTerminal } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { StatHistogram } from "@/components/layout/StatHistogram";
import { useDataStore } from "@/stores/dataStore";
import { useConsoleStore } from "@/stores/consoleStore";
import { useLayoutStore } from "@/stores/layoutStore";
import { useConfigStore } from "@/stores/configStore";
import { useConnectionStore } from "@/stores/connectionStore";
import { useStatsHistory } from "@/hooks/useStatsHistory";
import { useUnitFormat } from "@/hooks/useUnitFormat";
import { formatPercent } from "@/lib/formatBytes";
import { appLogger } from "@/lib/appLogger";
import { toggleDetailPanelWithSelection } from "@/lib/detailPanel";

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

interface MonitorStatProps {
  label: string;
  textValue: string;
  histogramValues: number[];
  histogramMax?: number;
  showHistogram: boolean;
}

function MonitorStat({
  label,
  textValue,
  histogramValues,
  histogramMax,
  showHistogram,
}: MonitorStatProps) {
  const showChart = showHistogram && histogramValues.length > 1;

  return (
    <div className="avar-footer__stat avar-footer__stat--monitor">
      <span className="avar-footer__stat-label">{label}</span>
      {showChart ? (
        <StatHistogram
          className="avar-footer__histogram"
          label={label}
          values={histogramValues}
          max={histogramMax}
          textValue={textValue}
        />
      ) : (
        <strong>{textValue}</strong>
      )}
    </div>
  );
}

export function Footer() {
  const { t } = useTranslation();
  const { formatBytes, formatTransferRate } = useUnitFormat();
  const health = useDataStore((s) => s.health);
  const stats = useDataStore((s) => s.stats);
  const visibleDownloadOrder = useDataStore((s) => s.visibleDownloadOrder);
  const connection = useConnectionStore((s) => s.connection);
  const footerMonitors = useConfigStore((s) => s.config.footerMonitors);
  const consoleOpen = useConsoleStore((s) => s.open);
  const hasUnseenErrors = useConsoleStore((s) => s.hasUnseenErrors);
  const toggleConsole = useConsoleStore((s) => s.toggleOpen);
  const detailPanelOpen = useLayoutStore((s) => s.detailPanelOpen);

  const monitorsEnabled =
    footerMonitors.disk || footerMonitors.memory || footerMonitors.cpu || footerMonitors.network;
  const showHistogram = footerMonitors.display === "histogram";
  const history = useStatsHistory(stats, monitorsEnabled && showHistogram);

  const monitorsAvailable = connection === "connected" && monitorsEnabled;

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
            {footerMonitors.disk && monitorsAvailable && stats ? (
              <div className="avar-footer__stat">
                <span className="avar-footer__stat-label">{t("health.disk")}</span>
                <strong>
                  {formatBytes(stats.diskFreeBytes)} / {formatBytes(stats.diskTotalBytes)} (
                  {formatPercent(diskUsedPercent)} {t("health.used")})
                </strong>
              </div>
            ) : null}
            {footerMonitors.memory && monitorsAvailable && stats ? (
              <MonitorStat
                label={t("health.memory")}
                textValue={`${formatBytes(stats.memoryUsedBytes)} / ${formatBytes(stats.memoryTotalBytes)} (${formatPercent(stats.memoryUsedPercent)})`}
                histogramValues={history.memory}
                histogramMax={100}
                showHistogram={showHistogram}
              />
            ) : null}
            {footerMonitors.cpu && monitorsAvailable && stats ? (
              <MonitorStat
                label={t("health.cpu")}
                textValue={formatPercent(stats.cpuUsagePercent)}
                histogramValues={history.cpu}
                histogramMax={100}
                showHistogram={showHistogram}
              />
            ) : null}
            {footerMonitors.network && monitorsAvailable && stats ? (
              <MonitorStat
                label={t("health.network")}
                textValue={formatTransferRate(stats.networkRxBytesPerSec)}
                histogramValues={history.network}
                showHistogram={showHistogram}
              />
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
          disabled={visibleDownloadOrder.length === 0}
          onClick={() => {
            appLogger.gui.debug("Detail panel toggled");
            toggleDetailPanelWithSelection();
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
