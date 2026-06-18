import { useCallback, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Input } from "@/components/ui/Input";
import { Select } from "@/components/ui/Select";
import { Button } from "@/components/ui/Button";
import { useConnectionStore } from "@/stores/connectionStore";
import { appLogger } from "@/lib/appLogger";

const CONFIG_DEFAULTS = {
  autoShutdown: "never",
  autoShutdownIdleSeconds: "60",
  logEnabled: "false",
  logPath: "",
} as const;

export function DaemonSettings() {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [autoShutdown, setAutoShutdown] = useState<string>(CONFIG_DEFAULTS.autoShutdown);
  const [autoShutdownIdleSeconds, setAutoShutdownIdleSeconds] = useState<string>(
    CONFIG_DEFAULTS.autoShutdownIdleSeconds,
  );
  const [logEnabled, setLogEnabled] = useState(false);
  const [logPath, setLogPath] = useState("");
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const load = useCallback(async () => {
    if (!client) {
      return;
    }
    try {
      setAutoShutdown(
        (await client.getConfig(
          "daemon.server.autoShutdown",
          CONFIG_DEFAULTS.autoShutdown,
        )) ?? CONFIG_DEFAULTS.autoShutdown,
      );
      setAutoShutdownIdleSeconds(
        (await client.getConfig(
          "daemon.server.autoShutdownIdleSeconds",
          CONFIG_DEFAULTS.autoShutdownIdleSeconds,
        )) ?? CONFIG_DEFAULTS.autoShutdownIdleSeconds,
      );
      setLogEnabled(
        (await client.getConfig("log.file.enabled", CONFIG_DEFAULTS.logEnabled)) === "true",
      );
      setLogPath(
        (await client.getConfig("log.file.path", CONFIG_DEFAULTS.logPath)) ??
          CONFIG_DEFAULTS.logPath,
      );
    } catch (err) {
      setError(err instanceof Error ? err.message : t("common.error"));
    }
  }, [client, t]);

  useEffect(() => {
    void load();
  }, [load]);

  async function save() {
    if (!client) {
      return;
    }
    setSaving(true);
    setError(null);
    try {
      await client.setConfig("daemon.server.autoShutdown", autoShutdown);
      await client.setConfig(
        "daemon.server.autoShutdownIdleSeconds",
        autoShutdownIdleSeconds,
      );
      await client.setConfig("log.file.enabled", logEnabled ? "true" : "false");
      await client.setConfig("log.file.path", logPath);
      appLogger.gui.info("Daemon settings saved");
    } catch (err) {
      setError(err instanceof Error ? err.message : t("common.error"));
    } finally {
      setSaving(false);
    }
  }

  return (
    <form className="avar-settings-form" onSubmit={(e) => e.preventDefault()}>
      <Select
        label={t("settings.daemon.autoShutdown")}
        value={autoShutdown}
        onChange={(e) => setAutoShutdown(e.target.value)}
      >
        <option value="never">{t("settings.daemon.autoShutdownNever")}</option>
        <option value="whenIdle">{t("settings.daemon.autoShutdownWhenIdle")}</option>
      </Select>

      {autoShutdown === "whenIdle" ? (
        <Input
          label={t("settings.daemon.autoShutdownIdleSeconds")}
          type="number"
          min={1}
          max={86400}
          value={autoShutdownIdleSeconds}
          onChange={(e) => setAutoShutdownIdleSeconds(e.target.value)}
        />
      ) : null}

      <section className="avar-settings-group">
        <h3 className="avar-settings-group__heading">{t("settings.daemon.fileLogging")}</h3>
        <label className="avar-checkbox-row">
          <input
            type="checkbox"
            checked={logEnabled}
            onChange={(e) => setLogEnabled(e.target.checked)}
          />
          {t("settings.daemon.logEnabled")}
        </label>
        <Input
          label={t("settings.daemon.logPath")}
          value={logPath}
          onChange={(e) => setLogPath(e.target.value)}
          disabled={!logEnabled}
        />
      </section>

      {error ? <p className="avar-field__error">{error}</p> : null}
      <Button loading={saving} onClick={() => void save()}>
        {t("common.save")}
      </Button>
    </form>
  );
}
