import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faXmark } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { useDraggable } from "@/hooks/useDraggable";
import { closePopupWindow, resolveConfirm } from "@/lib/popup";
import { usePopupStore } from "@/stores/popupStore";

function DraggablePopupWindow({
  id,
  title,
  url,
  width,
  height,
  alwaysOnTop,
}: {
  id: string;
  title: string;
  url: string;
  width: number;
  height: number;
  alwaysOnTop?: boolean;
}) {
  const { t } = useTranslation();
  const { dialogRef, dragHandleProps, dialogStyle } = useDraggable({ resetKey: id });

  return (
    <div
      ref={dialogRef}
      className={`avar-popup-window${alwaysOnTop ? " avar-popup-window--always-on-top" : ""}`}
      style={{ width, height, ...dialogStyle }}
      role="dialog"
      aria-label={title}
    >
      <header
        className="avar-popup-window__header avar-modal__header--draggable"
        {...dragHandleProps}
      >
        <span className="avar-popup-window__title">{title}</span>
        <Button
          size="sm"
          variant="ghost"
          aria-label={t("common.close")}
          onClick={() => closePopupWindow(id)}
        >
          <FontAwesomeIcon icon={faXmark} />
        </Button>
      </header>
      <iframe className="avar-popup-window__frame" src={url} title={title} />
    </div>
  );
}

export function PopupHost() {
  const windows = usePopupStore((s) => s.windows);
  const confirm = usePopupStore((s) => s.confirm);
  const [checkboxChecked, setCheckboxChecked] = useState(false);
  const { dialogRef, dragHandleProps, dialogStyle } = useDraggable({ resetKey: confirm?.id });

  useEffect(() => {
    setCheckboxChecked(confirm?.checkboxDefault ?? false);
  }, [confirm?.id, confirm?.checkboxDefault]);

  return (
    <>
      {windows.map((win) => (
        <DraggablePopupWindow
          key={win.id}
          id={win.id}
          title={win.title}
          url={win.url}
          width={win.width}
          height={win.height}
          alwaysOnTop={win.alwaysOnTop}
        />
      ))}

      {confirm ? (
        <div className="avar-modal-backdrop" role="presentation">
          <div
            ref={dialogRef}
            className="avar-modal"
            role="alertdialog"
            aria-modal="true"
            aria-labelledby="avar-confirm-title"
            style={dialogStyle}
          >
            <header className="avar-modal__header avar-modal__header--draggable" {...dragHandleProps}>
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
