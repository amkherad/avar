import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faPuzzlePiece } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { useConfigStore } from "@/stores/configStore";
import {
  BUNDLED_EXTENSION_VERSION,
  setExtensionBridgeSuspended,
  useExtensionBridgeStatus,
} from "@/lib/browserExtensionBridge";
import { copyTextToClipboard } from "@/lib/curlCommand";
import { appLogger } from "@/lib/appLogger";

export function ExtensionIntegrationButton() {
  const { t } = useTranslation();
  const updateConfig = useConfigStore((s) => s.updateConfig);
  const bridgeStatus = useExtensionBridgeStatus();
  const [open, setOpen] = useState(false);
  const [copied, setCopied] = useState(false);
  const containerRef = useRef<HTMLDivElement>(null);

  const isElectron = Boolean(window.avar?.isElectron);

  useEffect(() => {
    if (!open || !isElectron) {
      return;
    }

    function handleClickOutside(event: MouseEvent) {
      if (containerRef.current && !containerRef.current.contains(event.target as Node)) {
        setOpen(false);
      }
    }

    function handleEscape(event: KeyboardEvent) {
      if (event.key === "Escape") {
        setOpen(false);
      }
    }

    document.addEventListener("mousedown", handleClickOutside);
    document.addEventListener("keydown", handleEscape);
    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
      document.removeEventListener("keydown", handleEscape);
    };
  }, [open, isElectron]);

  if (!isElectron) {
    return null;
  }

  const statusLabel = !bridgeStatus.enabled
    ? t("extensionPanel.disabled")
    : bridgeStatus.suspended
      ? t("extensionPanel.suspended")
      : bridgeStatus.loading
        ? t("extensionPanel.checking")
        : bridgeStatus.connected
          ? t("extensionPanel.connected")
          : t("extensionPanel.disconnected");

  async function handleCopyUrl() {
    const ok = await copyTextToClipboard(bridgeStatus.bridgeUrl);
    if (ok) {
      appLogger.gui.debug("Extension bridge URL copied");
      setCopied(true);
      window.setTimeout(() => setCopied(false), 1500);
    }
  }

  return (
    <div className="avar-extension-panel" ref={containerRef}>
      <Button
        variant="ghost"
        size="sm"
        className="avar-extension-panel__trigger"
        aria-label={t("extensionPanel.aria")}
        aria-expanded={open}
        onClick={() => {
          appLogger.gui.debug("Extension panel", open ? "closed" : "opened");
          setOpen((prev) => !prev);
        }}
      >
        <span
          className={`avar-extension-panel__dot ${
            !bridgeStatus.enabled || bridgeStatus.suspended
              ? ""
              : bridgeStatus.connected
                ? "avar-extension-panel__dot--ok"
                : "avar-extension-panel__dot--error"
          }`}
          aria-hidden
        />
        <FontAwesomeIcon icon={faPuzzlePiece} />
      </Button>

      {open ? (
        <div className="avar-extension-panel__menu" role="dialog" aria-label={t("extensionPanel.title")}>
          <h3 className="avar-extension-panel__heading">{t("extensionPanel.title")}</h3>

          <div className="avar-extension-panel__status">
            <span
              className={`avar-connection__dot ${
                !bridgeStatus.enabled || bridgeStatus.suspended
                  ? ""
                  : bridgeStatus.connected
                    ? "avar-connection__dot--ok"
                    : "avar-connection__dot--error"
              }`}
              aria-hidden
            />
            <span>{statusLabel}</span>
          </div>

          <label className="avar-checkbox-row avar-extension-panel__toggle">
            <input
              type="checkbox"
              checked={bridgeStatus.enabled}
              onChange={(e) => updateConfig({ browserExtensionEnabled: e.target.checked })}
            />
            {t("extensionPanel.listen")}
          </label>

          {bridgeStatus.enabled ? (
            <div className="avar-extension-panel__suspend">
              {bridgeStatus.suspended ? (
                <Button
                  size="sm"
                  variant="primary"
                  type="button"
                  onClick={() => setExtensionBridgeSuspended(false)}
                >
                  {t("extensionPanel.resume")}
                </Button>
              ) : (
                <Button
                  size="sm"
                  variant="secondary"
                  type="button"
                  onClick={() => setExtensionBridgeSuspended(true)}
                >
                  {t("extensionPanel.suspend")}
                </Button>
              )}
              <p className="avar-extension-panel__suspend-hint">
                {bridgeStatus.suspended
                  ? t("extensionPanel.suspendedHint")
                  : t("extensionPanel.suspendHint")}
              </p>
            </div>
          ) : null}

          <div className="avar-extension-panel__row">
            <span className="avar-extension-panel__label">{t("extensionPanel.bridgeUrl")}</span>
            <code className="avar-extension-panel__code">{bridgeStatus.bridgeUrl}</code>
            <Button size="sm" variant="secondary" type="button" onClick={() => void handleCopyUrl()}>
              {copied ? t("common.copied") : t("extensionPanel.copyUrl")}
            </Button>
          </div>

          <dl className="avar-extension-panel__meta">
            <div>
              <dt>{t("extensionPanel.bridgeVersion")}</dt>
              <dd>{bridgeStatus.bridgeVersion}</dd>
            </div>
            <div>
              <dt>{t("extensionPanel.protocolVersion")}</dt>
              <dd>v{bridgeStatus.protocolVersion}</dd>
            </div>
            <div>
              <dt>{t("extensionPanel.extensionVersion")}</dt>
              <dd>{bridgeStatus.extensionVersion ?? t("extensionPanel.notConnected")}</dd>
            </div>
            <div>
              <dt>{t("extensionPanel.bundledVersion")}</dt>
              <dd>{BUNDLED_EXTENSION_VERSION}</dd>
            </div>
          </dl>

          {bridgeStatus.updateAvailable ? (
            <p className="avar-extension-panel__update">{t("extensionPanel.updateAvailable")}</p>
          ) : null}
        </div>
      ) : null}
    </div>
  );
}
