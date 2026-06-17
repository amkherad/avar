import { useTranslation } from "react-i18next";

import { Card } from "@/components/ui/Card";

import { Select } from "@/components/ui/Select";

import { Input } from "@/components/ui/Input";

import { useConfigStore } from "@/stores/configStore";

import type { LocaleId, SyncChannelId, ThemeId } from "@/config/defaults";

import i18n, { isRtlLocale } from "@/i18n";



export function SettingsPage() {

  const { t } = useTranslation();

  const config = useConfigStore((s) => s.config);

  const updateConfig = useConfigStore((s) => s.updateConfig);



  function setLocale(locale: LocaleId) {

    void i18n.changeLanguage(locale);

    document.documentElement.lang = locale;

    document.documentElement.dir = isRtlLocale(locale) ? "rtl" : "ltr";

    updateConfig({ locale });

  }



  function setSyncChannel(syncChannel: SyncChannelId) {

    updateConfig({ syncChannel });

  }



  return (

    <div className="avar-page-scroll">

    <Card title={t("settings.title")}>

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

      </form>

    </Card>

    </div>

  );

}


