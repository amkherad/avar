import { useState } from "react";

import { useTranslation } from "react-i18next";

import { Button } from "@/components/ui/Button";

import { BrowserIcon } from "@/components/ui/BrowserIcon";

import { useConfigStore } from "@/stores/configStore";

import { useConnectionStore } from "@/stores/connectionStore";

import {

  detectCurrentBrowser,

  installBrowserExtension,

  type BrowserId,

} from "@/lib/browserExtensionInstall";

import { requestNotificationPermission } from "@/lib/notificationService";

import {

  getExtensionGuiUrl,

  useExtensionBridgeStatus,

} from "@/lib/browserExtensionBridge";



const BROWSERS: BrowserId[] = ["chrome", "firefox", "edge", "opera"];



export function BrowserIntegrationSettings() {

  const { t } = useTranslation();

  const config = useConfigStore((s) => s.config);

  const updateConfig = useConfigStore((s) => s.updateConfig);

  const sessions = config.sessions;

  const activeSessionId = config.activeSessionId;

  const connection = useConnectionStore((s) => s.connection);

  const bridgeStatus = useExtensionBridgeStatus();

  const [status, setStatus] = useState<string | null>(null);

  const [installing, setInstalling] = useState<BrowserId | null>(null);



  const activeSession = sessions.find((session) => session.id === activeSessionId) ?? sessions[0];

  const currentBrowser = detectCurrentBrowser();

  const guiUrl = getExtensionGuiUrl();



  async function handleInstall(browser: BrowserId) {

    setInstalling(browser);

    setStatus(null);

    try {

      await installBrowserExtension(browser);

      setStatus(t("settings.browser.installStarted", { browser: t(`settings.browser.names.${browser}`) }));

    } catch (error) {

      setStatus(error instanceof Error ? error.message : t("common.error"));

    } finally {

      setInstalling(null);

    }

  }



  async function handleCopyGuiUrl() {

    await navigator.clipboard.writeText(guiUrl);

    setStatus(t("settings.browser.guiUrlCopied"));

  }



  async function handleEnableNotifications() {

    const permission = await requestNotificationPermission();

    if (permission === "granted") {

      setStatus(t("settings.pwa.notificationsGranted"));

    } else if (permission === "denied") {

      setStatus(t("settings.pwa.notificationsDenied"));

    } else {

      setStatus(t("settings.pwa.notificationsUnsupported"));

    }

  }



  const extensionStatusLabel = !config.browserExtensionEnabled

    ? t("settings.browser.extensionDisabled")

    : bridgeStatus.loading

      ? t("settings.browser.extensionChecking")

      : bridgeStatus.connected

        ? t("settings.browser.extensionConnected")

        : t("settings.browser.extensionDisconnected");



  return (

    <form className="avar-settings-form" onSubmit={(e) => e.preventDefault()}>

      <section className="avar-settings-group">

        <h3 className="avar-settings-group__heading">{t("settings.browser.title")}</h3>

        <p className="avar-settings-hint">{t("settings.browser.hint")}</p>



        <label className="avar-checkbox-row avar-settings-browser__toggle">

          <input

            type="checkbox"

            checked={config.browserExtensionEnabled}

            onChange={(e) => updateConfig({ browserExtensionEnabled: e.target.checked })}

          />

          {t("settings.browser.enableListener")}

        </label>



        <div className="avar-settings-browser__status">

          <span

            className={`avar-connection__dot ${

              !config.browserExtensionEnabled

                ? ""

                : bridgeStatus.connected

                  ? "avar-connection__dot--ok"

                  : "avar-connection__dot--error"

            }`}

            aria-hidden

          />

          <span className="avar-settings-hint">{extensionStatusLabel}</span>

        </div>



        <div className="avar-settings-browser__session">

          <span className="avar-settings-hint">{t("settings.browser.guiUrl")}</span>

          <code className="avar-settings-browser__url">{guiUrl}</code>

          <Button size="sm" variant="secondary" type="button" onClick={() => void handleCopyGuiUrl()}>

            {t("settings.browser.copyGuiUrl")}

          </Button>

        </div>



        {activeSession ? (

          <div className="avar-settings-browser__session">

            <span className="avar-settings-hint">{t("settings.browser.daemonUrl")}</span>

            <code className="avar-settings-browser__url">{activeSession.baseUrl}</code>

          </div>

        ) : null}



        {connection !== "connected" ? (

          <p className="avar-settings-hint avar-settings-hint--warn">{t("settings.browser.daemonOffline")}</p>

        ) : null}



        <div className="avar-browser-grid">

          {BROWSERS.map((browser) => (

            <button

              key={browser}

              type="button"

              className={`avar-browser-card ${currentBrowser === browser ? "avar-browser-card--current" : ""}`}

              disabled={installing !== null}

              onClick={() => void handleInstall(browser)}

            >

              <BrowserIcon browser={browser} className="avar-browser-card__icon" />

              <span className="avar-browser-card__name">{t(`settings.browser.names.${browser}`)}</span>

              <span className="avar-browser-card__action">

                {installing === browser ? t("common.loading") : t("settings.browser.install")}

              </span>

            </button>

          ))}

        </div>



        <ol className="avar-settings-steps">

          <li>{t("settings.browser.stepDownload")}</li>

          <li>{t("settings.browser.stepExtract")}</li>

          <li>{t("settings.browser.stepLoad")}</li>

          <li>{t("settings.browser.stepConfigure")}</li>

        </ol>

      </section>



      <section className="avar-settings-group">

        <h3 className="avar-settings-group__heading">{t("settings.pwa.notificationsTitle")}</h3>

        <p className="avar-settings-hint">{t("settings.pwa.notificationsHint")}</p>

        <Button type="button" variant="secondary" onClick={() => void handleEnableNotifications()}>

          {t("settings.pwa.enableNotifications")}

        </Button>

      </section>



      {status ? <p className="avar-settings-status">{status}</p> : null}

    </form>

  );

}

