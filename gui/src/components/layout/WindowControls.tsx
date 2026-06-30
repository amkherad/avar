import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faSquare, faWindowMaximize, faWindowMinimize, faXmark } from "@fortawesome/free-solid-svg-icons";

export function WindowControls() {
  const { t } = useTranslation();
  const [maximized, setMaximized] = useState(false);

  useEffect(() => {
    if (!window.avar?.isElectron || !window.avar.isWindowMaximized) {
      return;
    }

    let cancelled = false;

    async function refreshMaximized() {
      const value = await window.avar!.isWindowMaximized!();
      if (!cancelled) {
        setMaximized(value);
      }
    }

    void refreshMaximized();
    const timer = window.setInterval(() => {
      void refreshMaximized();
    }, 500);

    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, []);

  if (!window.avar?.isElectron) {
    return null;
  }

  async function handleMinimize() {
    await window.avar?.minimizeWindow?.();
  }

  async function handleMaximize() {
    await window.avar?.maximizeWindow?.();
    if (window.avar?.isWindowMaximized) {
      setMaximized(await window.avar.isWindowMaximized());
    }
  }

  async function handleClose() {
    await window.avar?.closeWindow?.();
  }

  return (
    <div className="avar-window-controls" role="group" aria-label={t("window.controls")}>
      <button
        type="button"
        className="avar-window-controls__btn"
        aria-label={t("window.minimize")}
        title={t("window.minimize")}
        onClick={() => void handleMinimize()}
      >
        <FontAwesomeIcon icon={faWindowMinimize} />
      </button>
      <button
        type="button"
        className="avar-window-controls__btn"
        aria-label={maximized ? t("window.restore") : t("window.maximize")}
        title={maximized ? t("window.restore") : t("window.maximize")}
        onClick={() => void handleMaximize()}
      >
        <FontAwesomeIcon icon={maximized ? faSquare : faWindowMaximize} />
      </button>
      <button
        type="button"
        className="avar-window-controls__btn avar-window-controls__btn--close"
        aria-label={t("window.close")}
        title={t("window.close")}
        onClick={() => void handleClose()}
      >
        <FontAwesomeIcon icon={faXmark} />
      </button>
    </div>
  );
}
