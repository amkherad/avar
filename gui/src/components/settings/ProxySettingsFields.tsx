import { useTranslation } from "react-i18next";
import { Input } from "@/components/ui/Input";
import { Select } from "@/components/ui/Select";
import type { ProxySettings, ProxyType } from "@/lib/proxySettings";

export interface ProxySettingsFieldsProps {
  value: ProxySettings;
  onChange: (value: ProxySettings) => void;
  disabled?: boolean;
}

export function ProxySettingsFields({ value, onChange, disabled = false }: ProxySettingsFieldsProps) {
  const { t } = useTranslation();

  function patch(partial: Partial<ProxySettings>) {
    onChange({ ...value, ...partial });
  }

  return (
    <fieldset className="avar-proxy-settings" disabled={disabled}>
      <legend className="avar-proxy-settings__legend">{t("proxy.title")}</legend>

      <label className="avar-checkbox-row">
        <input
          type="checkbox"
          checked={value.enabled}
          onChange={(e) => patch({ enabled: e.target.checked })}
        />
        {t("proxy.enabled")}
      </label>

      {value.enabled ? (
        <div className="avar-proxy-settings__fields">
          <Select
            label={t("proxy.type")}
            value={value.type}
            onChange={(e) => patch({ type: e.target.value as ProxyType })}
          >
            <option value="http">{t("proxy.types.http")}</option>
            <option value="https">{t("proxy.types.https")}</option>
            <option value="socks5">{t("proxy.types.socks5")}</option>
          </Select>

          <div className="avar-proxy-settings__row">
            <Input
              label={t("proxy.host")}
              value={value.host}
              onChange={(e) => patch({ host: e.target.value })}
              placeholder="127.0.0.1"
            />
            <Input
              label={t("proxy.port")}
              value={value.port}
              onChange={(e) => patch({ port: e.target.value })}
              placeholder="8080"
              inputMode="numeric"
            />
          </div>

          <Input
            label={t("proxy.username")}
            value={value.username}
            onChange={(e) => patch({ username: e.target.value })}
            autoComplete="off"
          />
          <Input
            label={t("proxy.password")}
            type="password"
            value={value.password}
            onChange={(e) => patch({ password: e.target.value })}
            autoComplete="new-password"
          />
        </div>
      ) : null}
    </fieldset>
  );
}
