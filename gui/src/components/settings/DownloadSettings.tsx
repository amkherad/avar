import { useCallback, useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { Input } from "@/components/ui/Input";
import { DirectoryPathInput } from "@/components/ui/DirectoryPathInput";
import { Select } from "@/components/ui/Select";
import { ProxySettingsFields } from "@/components/settings/ProxySettingsFields";
import { defaultProxySettings, type ProxySettings } from "@/lib/proxySettings";
import {
  DOWNLOAD_CONFIG_DEFAULTS,
  DOWNLOAD_CONFIG_FIELD_KEYS,
  DOWNLOAD_PROXY_CONFIG_KEYS,
  defaultDownloadConfigValues,
} from "@/lib/daemonConfigDefaults";
import {
  daemonConfigLoadWarning,
  daemonConfigSaveError,
  loadDaemonConfigValues,
  saveDaemonConfigValue,
} from "@/lib/daemonConfigSettings";
import { useDaemonConfigPersist } from "@/hooks/useDaemonConfigPersist";
import { useDaemonDirectoryPathMode } from "@/hooks/useDirectoryPathMode";
import { useConnectionStore } from "@/stores/connectionStore";

const SIZE_UNIT_OPTIONS = ["Bytes", "KiB", "MiB", "GiB"] as const;
const SPEED_UNIT_OPTIONS = [
  "Bytes/s",
  "bits/s",
  "KiB/s",
  "MiB/s",
  "GiB/s",
  "Kib/s",
  "Mib/s",
  "Gib/s",
] as const;

function normalizeUnitOption<T extends readonly string[]>(
  value: string | null | undefined,
  options: T,
  fallback: T[number],
): T[number] {
  if (value != null && (options as readonly string[]).includes(value)) {
    return value as T[number];
  }
  return fallback;
}

function proxyFromConfig(values: Record<string, string>): ProxySettings {
  return {
    enabled: values["dm.proxy.enabled"] === "true",
    type: (values["dm.proxy.type"] as ProxySettings["type"]) || "http",
    host: values["dm.proxy.host"] ?? "",
    port: values["dm.proxy.port"] ?? "",
    username: values["dm.proxy.username"] ?? "",
    password: values["dm.proxy.password"] ?? "",
    noProxy: values["dm.proxy.noProxy"] ?? "",
  };
}

export function DownloadSettings() {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const directoryPathMode = useDaemonDirectoryPathMode();
  const [values, setValues] = useState(defaultDownloadConfigValues);
  const [proxy, setProxy] = useState<ProxySettings>(defaultProxySettings);
  const [error, setError] = useState<string | null>(null);
  const [warning, setWarning] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [ready, setReady] = useState(false);
  const loadGenRef = useRef(0);

  const { schedulePersist } = useDaemonConfigPersist({
    client,
    ready,
    onSaveError: (key, err) => setError(daemonConfigSaveError(t, key, err)),
    onSaveSuccess: () => setError(null),
  });

  const persistProxy = useCallback(
    async (next: ProxySettings) => {
      if (!client || !ready) {
        if (!client) {
          setError(t("settings.backendDisconnected"));
        }
        return;
      }
      try {
        await saveDaemonConfigValue(client, "dm.proxy.enabled", next.enabled ? "true" : "false");
        await saveDaemonConfigValue(client, "dm.proxy.type", next.type);
        await saveDaemonConfigValue(client, "dm.proxy.host", next.host);
        await saveDaemonConfigValue(client, "dm.proxy.port", next.port);
        await saveDaemonConfigValue(client, "dm.proxy.username", next.username);
        await saveDaemonConfigValue(client, "dm.proxy.password", next.password);
        await saveDaemonConfigValue(client, "dm.proxy.noProxy", next.noProxy ?? "");
        setError(null);
      } catch (err) {
        setError(daemonConfigSaveError(t, "dm.proxy", err));
      }
    },
    [client, ready, t],
  );

  const load = useCallback(async () => {
    if (!client) {
      setReady(false);
      setValues(defaultDownloadConfigValues());
      setProxy(defaultProxySettings());
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
      const fieldResult = await loadDaemonConfigValues(
        client,
        DOWNLOAD_CONFIG_FIELD_KEYS,
        DOWNLOAD_CONFIG_DEFAULTS,
      );
      const proxyResult = await loadDaemonConfigValues(
        client,
        DOWNLOAD_PROXY_CONFIG_KEYS,
        DOWNLOAD_CONFIG_DEFAULTS,
      );

      if (loadGen !== loadGenRef.current) {
        return;
      }

      const next: Record<string, string> = { ...fieldResult.values, ...proxyResult.values };
      for (const key of DOWNLOAD_CONFIG_FIELD_KEYS) {
        if (key === "dm.progress.sizeUnit") {
          next[key] = normalizeUnitOption(
            next[key],
            SIZE_UNIT_OPTIONS,
            DOWNLOAD_CONFIG_DEFAULTS[key],
          );
        } else if (key === "dm.progress.speedUnit") {
          next[key] = normalizeUnitOption(
            next[key],
            SPEED_UNIT_OPTIONS,
            DOWNLOAD_CONFIG_DEFAULTS[key],
          );
        }
      }

      const failedKeys = [...fieldResult.failedKeys, ...proxyResult.failedKeys];
      setValues(next as ReturnType<typeof defaultDownloadConfigValues>);
      setProxy(proxyFromConfig(next));
      setWarning(daemonConfigLoadWarning(t, failedKeys));
    } catch (err) {
      if (loadGen === loadGenRef.current) {
        setValues(defaultDownloadConfigValues());
        setProxy(defaultProxySettings());
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

  function updateField(key: string, value: string) {
    setValues((prev) => ({ ...prev, [key]: value }));
  }

  function setField(key: string, value: string, immediate = false) {
    updateField(key, value);
    schedulePersist(key, value, immediate);
  }

  function setNumericField(key: string, value: string) {
    updateField(key, value);
    schedulePersist(key, value);
  }

  function commitNumericField(key: string, value: string) {
    updateField(key, value);
    schedulePersist(key, value, true);
  }

  function updateProxy(next: ProxySettings) {
    setProxy(next);
    void persistProxy(next);
  }

  return (
    <form className="avar-settings-form" onSubmit={(e) => e.preventDefault()}>
      <fieldset className="avar-settings-form__fieldset" disabled={loading}>
        <DirectoryPathInput
          mode={directoryPathMode}
          label={t("settings.download.tempPath")}
          value={values["dm.tempPath"] ?? ""}
          onChange={(next) => setField("dm.tempPath", next)}
        />
        <DirectoryPathInput
          mode={directoryPathMode}
          label={t("settings.download.downloadPath")}
          value={values["dm.downloadPath"] ?? ""}
          onChange={(next) => setField("dm.downloadPath", next)}
        />

        <section className="avar-settings-group">
          <h3 className="avar-settings-group__heading">{t("settings.download.segmentation")}</h3>
          <label className="avar-checkbox-row">
            <input
              type="checkbox"
              checked={values["dm.segmentation.enabled"] === "true"}
              onChange={(e) =>
                setField("dm.segmentation.enabled", e.target.checked ? "true" : "false", true)
              }
            />
            {t("settings.download.segmentationEnabled")}
          </label>
          <Select
            label={t("settings.download.segmentationStrategy")}
            value={values["dm.segmentation.strategy"] ?? DOWNLOAD_CONFIG_DEFAULTS["dm.segmentation.strategy"]}
            onChange={(e) => setField("dm.segmentation.strategy", e.target.value, true)}
          >
            <option value="balanced">{t("settings.download.strategyBalanced")}</option>
            <option value="left-heavy">{t("settings.download.strategyLeftHeavy")}</option>
          </Select>
          <Input
            label={t("settings.download.concurrency")}
            type="number"
            min={1}
            value={values["dm.segmentation.concurrency"] ?? ""}
            onChange={(e) => setNumericField("dm.segmentation.concurrency", e.target.value)}
            onBlur={(e) => commitNumericField("dm.segmentation.concurrency", e.target.value)}
          />
          <Input
            label={t("settings.download.chunkSize")}
            type="number"
            min={1}
            value={values["dm.segmentation.chunkSize"] ?? ""}
            onChange={(e) => setNumericField("dm.segmentation.chunkSize", e.target.value)}
            onBlur={(e) => commitNumericField("dm.segmentation.chunkSize", e.target.value)}
          />
          <Input
            label={t("settings.download.minFileSize")}
            type="number"
            min={1}
            value={values["dm.segmentation.minFileSize"] ?? ""}
            onChange={(e) => setNumericField("dm.segmentation.minFileSize", e.target.value)}
            onBlur={(e) => commitNumericField("dm.segmentation.minFileSize", e.target.value)}
          />
        </section>

        <section className="avar-settings-group">
          <h3 className="avar-settings-group__heading">{t("settings.download.progress")}</h3>
          <Select
            label={t("settings.download.sizeUnit")}
            value={values["dm.progress.sizeUnit"] ?? DOWNLOAD_CONFIG_DEFAULTS["dm.progress.sizeUnit"]}
            onChange={(e) => setField("dm.progress.sizeUnit", e.target.value, true)}
          >
            {SIZE_UNIT_OPTIONS.map((unit) => (
              <option key={unit} value={unit}>
                {unit}
              </option>
            ))}
          </Select>
          <Select
            label={t("settings.download.speedUnit")}
            value={values["dm.progress.speedUnit"] ?? DOWNLOAD_CONFIG_DEFAULTS["dm.progress.speedUnit"]}
            onChange={(e) => setField("dm.progress.speedUnit", e.target.value, true)}
          >
            {SPEED_UNIT_OPTIONS.map((unit) => (
              <option key={unit} value={unit}>
                {unit}
              </option>
            ))}
          </Select>
          <Select
            label={t("settings.download.progressStyle")}
            value={values["dm.progress.style"] ?? DOWNLOAD_CONFIG_DEFAULTS["dm.progress.style"]}
            onChange={(e) => setField("dm.progress.style", e.target.value, true)}
          >
            <option value="segmented">{t("settings.download.progressSegmented")}</option>
            <option value="aggregate">{t("settings.download.progressAggregate")}</option>
          </Select>
        </section>

        <ProxySettingsFields value={proxy} onChange={updateProxy} showNoProxy disabled={loading} />

        {warning ? <p className="avar-settings-hint">{warning}</p> : null}
        {error ? <p className="avar-field__error">{error}</p> : null}
      </fieldset>
    </form>
  );
}
