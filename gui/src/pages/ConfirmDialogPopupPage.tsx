import { useEffect, useState } from "react";
import { Button } from "@/components/ui/Button";
import {
  readStashedConfirm,
  resolveConfirmInPopup,
  type ConfirmDialogOptions,
} from "@/lib/popup";

export interface ConfirmDialogPopupPageProps {
  confirmId: string;
}

export function ConfirmDialogPopupPage({ confirmId }: ConfirmDialogPopupPageProps) {
  const [options, setOptions] = useState<ConfirmDialogOptions | null>(() =>
    readStashedConfirm(confirmId),
  );
  const [checkboxChecked, setCheckboxChecked] = useState(
    () => options?.checkboxDefault ?? false,
  );

  useEffect(() => {
    const stashed = readStashedConfirm(confirmId);
    setOptions(stashed);
    setCheckboxChecked(stashed?.checkboxDefault ?? false);
    if (stashed?.title) {
      document.title = stashed.title;
    }
  }, [confirmId]);

  if (!options) {
    return (
      <div className="avar-popup-page avar-confirm-popup">
        <p className="avar-empty">Dialog not found.</p>
      </div>
    );
  }

  const finish = (confirmed: boolean) => {
    resolveConfirmInPopup(confirmId, { confirmed, checkboxChecked });
    window.close();
  };

  return (
    <div className="avar-popup-page avar-confirm-popup">
      <div className="avar-confirm-popup__card" role="alertdialog" aria-modal="true">
        <header className="avar-confirm-popup__header">
          <h2>{options.title}</h2>
        </header>
        <div className="avar-confirm-popup__body">
          <p>{options.message}</p>
          {options.checkboxLabel ? (
            <label className="avar-checkbox-row">
              <input
                type="checkbox"
                checked={checkboxChecked}
                onChange={(e) => setCheckboxChecked(e.target.checked)}
              />
              {options.checkboxLabel}
            </label>
          ) : null}
        </div>
        <footer className="avar-confirm-popup__footer">
          <Button variant="ghost" onClick={() => finish(false)}>
            {options.cancelLabel ?? "Cancel"}
          </Button>
          <Button variant="danger" onClick={() => finish(true)}>
            {options.confirmLabel ?? "OK"}
          </Button>
        </footer>
      </div>
    </div>
  );
}
