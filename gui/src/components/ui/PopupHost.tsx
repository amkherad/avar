import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faXmark } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { closePopupWindow, resolveConfirm } from "@/lib/popup";
import { usePopupStore } from "@/stores/popupStore";

export function PopupHost() {
  const { t } = useTranslation();
  const windows = usePopupStore((s) => s.windows);
  const confirm = usePopupStore((s) => s.confirm);
  const [checkboxChecked, setCheckboxChecked] = useState(false);

  useEffect(() => {
    setCheckboxChecked(confirm?.checkboxDefault ?? false);
  }, [confirm?.id, confirm?.checkboxDefault]);

  return (
    <>
      {windows.map((win) => (
        <div
          key={win.id}
          className="avar-popup-window"
          style={{ width: win.width, height: win.height }}
          role="dialog"
          aria-label={win.title}
        >
          <header className="avar-popup-window__header">
            <span className="avar-popup-window__title">{win.title}</span>
            <Button
              size="sm"
              variant="ghost"
              aria-label={t("common.close")}
              onClick={() => closePopupWindow(win.id)}
            >
              <FontAwesomeIcon icon={faXmark} />
            </Button>
          </header>
          <iframe
            className="avar-popup-window__frame"
            src={win.url}
            title={win.title}
          />
        </div>
      ))}

      {confirm ? (
        <div className="avar-modal-backdrop" role="presentation">
          <div
            className="avar-modal"
            role="alertdialog"
            aria-modal="true"
            aria-labelledby="avar-confirm-title"
          >
            <header className="avar-modal__header">
              <h2 id="avar-confirm-title">{confirm.title}</h2>
            </header>
            <div className="avar-modal__body">
              <p>{confirm.message}</p>
              {confirm.checkboxLabel ? (
                <label className="avar-checkbox-row">
                  <input
                    type="checkbox"
                    checked={checkboxChecked}
                    onChange={(e) => setCheckboxChecked(e.target.checked)}
                  />
                  {confirm.checkboxLabel}
                </label>
              ) : null}
            </div>
            <footer className="avar-modal__footer">
              <Button variant="ghost" onClick={() => resolveConfirm(false)}>
                {confirm.cancelLabel}
              </Button>
              <Button
                variant="danger"
                onClick={() => resolveConfirm(true, checkboxChecked)}
              >
                {confirm.confirmLabel}
              </Button>
            </footer>
          </div>
        </div>
      ) : null}
    </>
  );
}
