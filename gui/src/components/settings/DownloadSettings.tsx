import { useCallback, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Input } from "@/components/ui/Input";
import { DirectoryPathInput } from "@/components/ui/DirectoryPathInput";
import { Select } from "@/components/ui/Select";
import { Button } from "@/components/ui/Button";
import { ProxySettingsFields } from "@/components/settings/ProxySettingsFields";
import { defaultProxySettings, type ProxySettings } from "@/lib/proxySettings";
import { useDaemonDirectoryPathMode } from "@/hooks/useDirectoryPathMode";
import { useConnectionStore } from "@/stores/connectionStore";
import { appLogger } from "@/lib/appLogger";

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

const CONFIG_DEFAULTS = {
  "dm.segmentation.enabled": "true",
  "dm.segmentation.strategy": "balanced",
  "dm.segmentation.concurrency": "4",
  "dm.segmentation.chunkSize": "262144",
  "dm.segmentation.minFileSize": "1048576",
  "dm.tempPath": "",
  "dm.downloadPath": "",
  "dm.progress.sizeUnit": "MiB",
  "dm.progress.speedUnit": "MiB/s",
  "dm.progress.style": "segmented",
  "dm.proxy.enabled": "false",
  "dm.proxy.type": "http",
  "dm.proxy.host": "",
  "dm.proxy.port": "",
  "dm.proxy.username": "",
  "dm.proxy.password": "",
  "dm.proxy.noProxy": "",
} as const;

const SEGMENT_KEYS = [
  "dm.segmentation.enabled",
  "dm.segmentation.strategy",
  "dm.segmentation.concurrency",
  "dm.segmentation.chunkSize",
  "dm.segmentation.minFileSize",
  "dm.tempPath",
  "dm.downloadPath",
  "dm.progress.sizeUnit",
  "dm.progress.speedUnit",
  "dm.progress.style",
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

export function DownloadSettings() {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const directoryPathMode = useDaemonDirectoryPathMode();
  const [values, setValues] = useState<Record<string, string>>({});
  const [proxy, setProxy] = useState<ProxySettings>(defaultProxySettings());
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [saved, setSaved] = useState(false);

  const load = useCallback(async () => {
    if (!client) {
      return;
    }
    try {
      const next: Record<string, string> = {};
      for (const key of SEGMENT_KEYS) {
        const defaultValue = CONFIG_DEFAULTS[key];
        const raw = (await client.getConfig(key, defaultValue)) ?? defaultValue;
        if (key === "dm.progress.sizeUnit") {
          next[key] = normalizeUnitOption(raw, SIZE_UNIT_OPTIONS, CONFIG_DEFAULTS[key]);
        } else if (key === "dm.progress.speedUnit") {
          next[key] = normalizeUnitOption(raw, SPEED_UNIT_OPTIONS, CONFIG_DEFAULTS[key]);
        } else {
          next[key] = raw;
        }
      }
      setValues(next);

      const enabled =
        (await client.getConfig("dm.proxy.enabled", CONFIG_DEFAULTS["dm.proxy.enabled"])) ===
        "true";
      setProxy({
        enabled,
        type:
          ((await client.getConfig("dm.proxy.type", CONFIG_DEFAULTS["dm.proxy.type"])) as ProxySettings["type"]) ||
          "http",
        host:
          (await client.getConfig("dm.proxy.host", CONFIG_DEFAULTS["dm.proxy.host"])) ??
          CONFIG_DEFAULTS["dm.proxy.host"],
        port:
          (await client.getConfig("dm.proxy.port", CONFIG_DEFAULTS["dm.proxy.port"])) ??
          CONFIG_DEFAULTS["dm.proxy.port"],
        username:
          (await client.getConfig("dm.proxy.username", CONFIG_DEFAULTS["dm.proxy.username"])) ??
          CONFIG_DEFAULTS["dm.proxy.username"],
        password:
          (await client.getConfig("dm.proxy.password", CONFIG_DEFAULTS["dm.proxy.password"])) ??
          CONFIG_DEFAULTS["dm.proxy.password"],
        noProxy:
          (await client.getConfig("dm.proxy.noProxy", CONFIG_DEFAULTS["dm.proxy.noProxy"])) ??
          CONFIG_DEFAULTS["dm.proxy.noProxy"],
      });
    } catch (err) {
      setError(err instanceof Error ? err.message : t("common.error"));
    }
  }, [client, t]);

  useEffect(() => {
    void load();
  }, [load]);

  async function save() {
    if (!client) {
      setError(t("settings.backendDisconnected"));
      setSaved(false);
      return;
    }
    setSaving(true);
    setError(null);
    setSaved(false);
    try {
      for (const key of SEGMENT_KEYS) {
        if (values[key] !== undefined) {
          await client.setConfig(key, values[key]);
        }
      }
      await client.setConfig("dm.proxy.enabled", proxy.enabled ? "true" : "false");
      await client.setConfig("dm.proxy.type", proxy.type);
      await client.setConfig("dm.proxy.host", proxy.host);
      await client.setConfig("dm.proxy.port", proxy.port);
      await client.setConfig("dm.proxy.username", proxy.username);
      await client.setConfig("dm.proxy.password", proxy.password);
      await client.setConfig("dm.proxy.noProxy", proxy.noProxy ?? "");
      appLogger.gui.info("Download settings saved");
      setSaved(true);
    } catch (err) {
      setError(err instanceof Error ? err.message : t("common.error"));
    } finally {
      setSaving(false);
    }
  }

  function setField(key: string, value: string) {
    setSaved(false);
    setValues((prev) => ({ ...prev, [key]: value }));
  }

  return (
    <form className="avar-settings-form" onSubmit={(e) => e.preventDefault()}>
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
              setField("dm.segmentation.enabled", e.target.checked ? "true" : "false")
            }
          />
          {t("settings.download.segmentationEnabled")}
        </label>
        <Select
          label={t("settings.download.segmentationStrategy")}
          value={values["dm.segmentation.strategy"] ?? "balanced"}
          onChange={(e) => setField("dm.segmentation.strategy", e.target.value)}
        >
          <option value="balanced">{t("settings.download.strategyBalanced")}</option>
          <option value="left-heavy">{t("settings.download.strategyLeftHeavy")}</option>
        </Select>
        <Input
          label={t("settings.download.concurrency")}
          type="number"
          min={1}
          value={values["dm.segmentation.concurrency"] ?? ""}
          onChange={(e) => setField("dm.segmentation.concurrency", e.target.value)}
        />
        <Input
          label={t("settings.download.chunkSize")}
          type="number"
          min={1}
          value={values["dm.segmentation.chunkSize"] ?? ""}
          onChange={(e) => setField("dm.segmentation.chunkSize", e.target.value)}
        />
        <Input
          label={t("settings.download.minFileSize")}
          type="number"
          min={1}
          value={values["dm.segmentation.minFileSize"] ?? ""}
          onChange={(e) => setField("dm.segmentation.minFileSize", e.target.value)}
        />
      </section>

      <section className="avar-settings-group">
        <h3 className="avar-settings-group__heading">{t("settings.download.progress")}</h3>
        <Select
          label={t("settings.download.sizeUnit")}
          value={values["dm.progress.sizeUnit"] ?? CONFIG_DEFAULTS["dm.progress.sizeUnit"]}
          onChange={(e) => setField("dm.progress.sizeUnit", e.target.value)}
        >
          {SIZE_UNIT_OPTIONS.map((unit) => (
            <option key={unit} value={unit}>
              {unit}
            </option>
          ))}
        </Select>
        <Select
          label={t("settings.download.speedUnit")}
          value={values["dm.progress.speedUnit"] ?? CONFIG_DEFAULTS["dm.progress.speedUnit"]}
          onChange={(e) => setField("dm.progress.speedUnit", e.target.value)}
        >
          {SPEED_UNIT_OPTIONS.map((unit) => (
            <option key={unit} value={unit}>
              {unit}
            </option>
          ))}
        </Select>
        <Select
          label={t("settings.download.progressStyle")}
          value={values["dm.progress.style"] ?? "segmented"}
          onChange={(e) => setField("dm.progress.style", e.target.value)}
        >
          <option value="segmented">{t("settings.download.progressSegmented")}</option>
          <option value="aggregate">{t("settings.download.progressAggregate")}</option>
        </Select>
      </section>

      <ProxySettingsFields
        value={proxy}
        onChange={(next) => {
          setSaved(false);
          setProxy(next);
        }}
        showNoProxy
      />

      {error ? <p className="avar-field__error">{error}</p> : null}
      {saved ? <p className="avar-settings-status">{t("settings.saved")}</p> : null}
      <Button loading={saving} onClick={() => void save()}>
        {t("common.save")}
      </Button>
    </form>
  );
}
