import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

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
        className="avar-window-controls__btn avar-window-controls__btn--minimize"
        aria-label={t("window.minimize")}
        title={t("window.minimize")}
        onClick={() => void handleMinimize()}
      >
        <span className="avar-window-controls__glyph avar-window-controls__glyph--minimize" aria-hidden />
      </button>
      <button
        type="button"
        className={`avar-window-controls__btn avar-window-controls__btn--maximize${maximized ? " avar-window-controls__btn--restore" : ""}`}
        aria-label={maximized ? t("window.restore") : t("window.maximize")}
        title={maximized ? t("window.restore") : t("window.maximize")}
        onClick={() => void handleMaximize()}
      >
        <span
          className={`avar-window-controls__glyph ${maximized ? "avar-window-controls__glyph--restore" : "avar-window-controls__glyph--maximize"}`}
          aria-hidden
        />
      </button>
      <button
        type="button"
        className="avar-window-controls__btn avar-window-controls__btn--close"
        aria-label={t("window.close")}
        title={t("window.close")}
        onClick={() => void handleClose()}
      >
        <span className="avar-window-controls__glyph avar-window-controls__glyph--close" aria-hidden />
      </button>
    </div>
  );
}
