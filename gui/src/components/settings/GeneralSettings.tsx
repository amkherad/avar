import { useTranslation } from "react-i18next";
import { Select } from "@/components/ui/Select";
import { Input } from "@/components/ui/Input";
import { Button } from "@/components/ui/Button";
import { useConfigStore } from "@/stores/configStore";
import type { FooterMonitorSettings, LocaleId, SyncChannelId, ThemeId } from "@/config/defaults";
import i18n, { isRtlLocale } from "@/i18n";
import { getBuildInfo } from "@/lib/buildInfo";
import { usePwaInstall } from "@/hooks/usePwaInstall";
import { isPwaSupported } from "@/lib/pwa";
import { requestNotificationPermission } from "@/lib/notificationService";

export function GeneralSettings() {
  const { t } = useTranslation();
  const config = useConfigStore((s) => s.config);
  const updateConfig = useConfigStore((s) => s.updateConfig);
  const build = getBuildInfo();
  const pwa = usePwaInstall();

  function setLocale(locale: LocaleId) {
    void i18n.changeLanguage(locale);
    document.documentElement.lang = locale;
    document.documentElement.dir = isRtlLocale(locale) ? "rtl" : "ltr";
    updateConfig({ locale });
  }

  function setSyncChannel(syncChannel: SyncChannelId) {
    updateConfig({ syncChannel });
  }

  function setFooterMonitor(key: keyof FooterMonitorSettings, enabled: boolean) {
    updateConfig({
      footerMonitors: { ...config.footerMonitors, [key]: enabled },
    });
  }

  return (
    <form className="avar-settings-form" onSubmit={(e) => e.preventDefault()}>
      <Select
        label={t("settings.theme")}
        value={config.theme}
        onChange={(e) => updateConfig({ theme: e.target.value as ThemeId })}
      >
        <option value="light">{t("settings.themeLight")}</option>
        <option value="dark">{t("settings.themeDark")}</option>
        <option value="system">{t("settings.themeSystem")}</option>
      </Select>

      <Select
        label={t("settings.language")}
        value={config.locale}
        onChange={(e) => setLocale(e.target.value as LocaleId)}
      >
        <option value="en">English</option>
        <option value="fa">فارسی</option>
      </Select>

      <Select
        label={t("settings.syncChannel")}
        value={config.syncChannel}
        onChange={(e) => setSyncChannel(e.target.value as SyncChannelId)}
      >
        <option value="poll">{t("settings.syncPoll")}</option>
        <option value="sse">{t("settings.syncSse")}</option>
        <option value="websocket">{t("settings.syncWebSocket")}</option>
      </Select>

      {config.syncChannel === "poll" ? (
        <Input
          label={t("settings.refresh")}
          type="number"
          min={1}
          max={120}
          value={Math.round(config.refreshIntervalMs / 1000)}
          onChange={(e) => {
            const seconds = Number(e.target.value);
            if (Number.isFinite(seconds) && seconds > 0) {
              updateConfig({ refreshIntervalMs: seconds * 1000 });
            }
          }}
        />
      ) : null}

      <Input
        label={t("settings.pingInterval")}
        type="number"
        min={1}
        max={30}
        value={Math.round(config.pingIntervalMs / 1000)}
        onChange={(e) => {
          const seconds = Number(e.target.value);
          if (Number.isFinite(seconds) && seconds > 0) {
            updateConfig({ pingIntervalMs: seconds * 1000 });
          }
        }}
      />

      <section className="avar-settings-group">
        <h3 className="avar-settings-group__heading">{t("settings.footerMonitors")}</h3>
        <p className="avar-settings-hint">{t("settings.footerMonitorsHint")}</p>
        <Select
          label={t("settings.footerMonitorDisplay")}
          value={config.footerMonitors.display}
          onChange={(e) =>
            updateConfig({
              footerMonitors: {
                ...config.footerMonitors,
                display: e.target.value as "text" | "histogram",
              },
            })
          }
        >
          <option value="text">{t("settings.footerMonitorDisplayText")}</option>
          <option value="histogram">{t("settings.footerMonitorDisplayHistogram")}</option>
        </Select>
        <div className="avar-settings-checkboxes">
          {(["disk", "memory", "cpu", "network"] as const).map((key) => (
            <label key={key} className="avar-checkbox-row">
              <input
                type="checkbox"
                checked={config.footerMonitors[key]}
                onChange={(e) => setFooterMonitor(key, e.target.checked)}
              />
              {t(`settings.footerMonitor.${key}`)}
            </label>
          ))}
        </div>
      </section>

      {isPwaSupported() ? (
        <section className="avar-settings-group">
          <h3 className="avar-settings-group__heading">{t("settings.pwa.title")}</h3>
          <p className="avar-settings-hint">{t("settings.pwa.hint")}</p>
          {pwa.installed ? (
            <p className="avar-settings-hint">{t("settings.pwa.installed")}</p>
          ) : pwa.canInstall ? (
            <Button type="button" onClick={() => void pwa.install()}>
              {t("settings.pwa.install")}
            </Button>
          ) : (
            <p className="avar-settings-hint">{t("settings.pwa.installUnavailable")}</p>
          )}
          <Button
            type="button"
            variant="secondary"
            onClick={() => void requestNotificationPermission()}
          >
            {t("settings.pwa.enableNotifications")}
          </Button>
        </section>
      ) : null}

      <section className="avar-settings-build">
        <h3 className="avar-settings-build__heading">{t("settings.buildInfo")}</h3>
        <dl className="avar-settings-build__list">
          <div>
            <dt>{t("settings.buildVersion")}</dt>
            <dd>{build.version}</dd>
          </div>
          {build.date ? (
            <div>
              <dt>{t("settings.buildDate")}</dt>
              <dd>{build.date}</dd>
            </div>
          ) : null}
          {build.commit ? (
            <div>
              <dt>{t("settings.buildCommit")}</dt>
              <dd className="avar-settings-build__mono">{build.commit}</dd>
            </div>
          ) : null}
        </dl>
      </section>
    </form>
  );
}
