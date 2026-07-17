import { useCallback, useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { Input } from "@/components/ui/Input";
import { DirectoryPathInput } from "@/components/ui/DirectoryPathInput";
import { Select } from "@/components/ui/Select";
import {
  DAEMON_CONFIG_DEFAULTS,
  DAEMON_CONFIG_FIELD_KEYS,
  defaultDaemonConfigValues,
} from "@/lib/daemonConfigDefaults";
import {
  daemonConfigLoadWarning,
  daemonConfigSaveError,
  loadDaemonConfigValues,
} from "@/lib/daemonConfigSettings";
import { useDaemonConfigPersist } from "@/hooks/useDaemonConfigPersist";
import { useDaemonDirectoryPathMode } from "@/hooks/useDirectoryPathMode";
import { useConnectionStore } from "@/stores/connectionStore";

export function DaemonSettings() {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [values, setValues] = useState(defaultDaemonConfigValues);
  const directoryPathMode = useDaemonDirectoryPathMode();
  const [loading, setLoading] = useState(false);
  const [ready, setReady] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [warning, setWarning] = useState<string | null>(null);
  const loadGenRef = useRef(0);

  const { schedulePersist } = useDaemonConfigPersist({
    client,
    ready,
    onSaveError: (key, err) => setError(daemonConfigSaveError(t, key, err)),
    onSaveSuccess: () => setError(null),
  });

  const load = useCallback(async () => {
    if (!client) {
      setReady(false);
      setValues(defaultDaemonConfigValues());
      setWarning(null);
      setError(t("settings.backendDisconnected"));
      return;
    }

    const loadGen = ++loadGenRef.current;
    setReady(false);
    setLoading(true);
    setError(null);
    setWarning(null);

    try {
      const result = await loadDaemonConfigValues(
        client,
        DAEMON_CONFIG_FIELD_KEYS,
        DAEMON_CONFIG_DEFAULTS,
      );

      if (loadGen !== loadGenRef.current) {
        return;
      }

      setValues(result.values as ReturnType<typeof defaultDaemonConfigValues>);
      setWarning(daemonConfigLoadWarning(t, result.failedKeys));
    } catch (err) {
      if (loadGen === loadGenRef.current) {
        setValues(defaultDaemonConfigValues());
        setError(err instanceof Error ? err.message : t("common.error"));
      }
    } finally {
      if (loadGen === loadGenRef.current) {
        setLoading(false);
        setReady(true);
      }
    }
  }, [client, t]);

  useEffect(() => {
    void load();
  }, [load]);

  function setField(key: keyof typeof DAEMON_CONFIG_DEFAULTS, value: string, immediate = false) {
    setValues((prev) => ({ ...prev, [key]: value }));
    schedulePersist(key, value, immediate);
  }

  const autoShutdown = values["daemon.server.autoShutdown"];
  const logEnabled = values["log.file.enabled"] === "true";
  const fileDownloadEnabled = values["daemon.server.fileDownload.enabled"] === "true";
  const fsBrowseEnabled = values["daemon.server.fsBrowse.enabled"] === "true";

  return (
    <form className="avar-settings-form" onSubmit={(e) => e.preventDefault()}>
      <fieldset className="avar-settings-form__fieldset" disabled={loading}>
        <Select
          label={t("settings.daemon.autoShutdown")}
          value={autoShutdown}
          onChange={(e) => setField("daemon.server.autoShutdown", e.target.value, true)}
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
            value={values["daemon.server.autoShutdownIdleSeconds"]}
            onChange={(e) =>
              setField("daemon.server.autoShutdownIdleSeconds", e.target.value)
            }
            onBlur={(e) =>
              setField("daemon.server.autoShutdownIdleSeconds", e.target.value, true)
            }
          />
        ) : null}

        <section className="avar-settings-group">
          <h3 className="avar-settings-group__heading">{t("settings.daemon.fileLogging")}</h3>
          <label className="avar-checkbox-row">
            <input
              type="checkbox"
              checked={logEnabled}
              onChange={(e) =>
                setField("log.file.enabled", e.target.checked ? "true" : "false", true)
              }
            />
            {t("settings.daemon.logEnabled")}
          </label>
          <DirectoryPathInput
            mode={directoryPathMode}
            label={t("settings.daemon.logPath")}
            value={values["log.file.path"]}
            onChange={(next) => setField("log.file.path", next)}
          />
        </section>

        <section className="avar-settings-group">
          <h3 className="avar-settings-group__heading">{t("settings.daemon.bookmarksFile")}</h3>
          <p className="avar-settings-hint">{t("settings.daemon.bookmarksFilePathHint")}</p>
          <DirectoryPathInput
            mode={directoryPathMode}
            label={t("settings.daemon.bookmarksFilePath")}
            value={values["bookmarks.file.path"]}
            onChange={(next) => setField("bookmarks.file.path", next.trim())}
          />
        </section>

        <section className="avar-settings-group">
          <h3 className="avar-settings-group__heading">{t("settings.daemon.fsBrowse")}</h3>
          <p className="avar-settings-hint">{t("settings.daemon.fsBrowseHint")}</p>
          <label className="avar-checkbox-row">
            <input
              type="checkbox"
              checked={fsBrowseEnabled}
              onChange={(e) =>
                setField("daemon.server.fsBrowse.enabled", e.target.checked ? "true" : "false", true)
              }
            />
            {t("settings.daemon.fsBrowseEnabled")}
          </label>
        </section>

        <section className="avar-settings-group">
          <h3 className="avar-settings-group__heading">{t("settings.daemon.remoteFileDownload")}</h3>
          <p className="avar-settings-hint">{t("settings.daemon.remoteFileDownloadHint")}</p>
          <label className="avar-checkbox-row">
            <input
              type="checkbox"
              checked={fileDownloadEnabled}
              onChange={(e) =>
                setField(
                  "daemon.server.fileDownload.enabled",
                  e.target.checked ? "true" : "false",
                  true,
                )
              }
            />
            {t("settings.daemon.fileDownloadEnabled")}
          </label>
        </section>

        {warning ? <p className="avar-settings-hint">{warning}</p> : null}
        {error ? <p className="avar-field__error">{error}</p> : null}
      </fieldset>
    </form>
  );
}
