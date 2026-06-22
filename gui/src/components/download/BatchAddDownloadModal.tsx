import { useState } from "react";
import { useTranslation } from "react-i18next";
import { Modal } from "@/components/ui/Modal";
import { Button } from "@/components/ui/Button";
import { parseUrlLinesToBatchItems } from "@/lib/batchAddDownloads";
import { openBatchAddPopup } from "@/lib/popup";
import { useDataStore, selectEffectiveQueueId } from "@/stores/dataStore";

export interface BatchAddDownloadModalProps {
  open: boolean;
  onClose: () => void;
}

export function BatchAddDownloadModal({ open, onClose }: BatchAddDownloadModalProps) {
  const { t } = useTranslation();
  const defaultQueueId = useDataStore(selectEffectiveQueueId);
  const [text, setText] = useState("");
  const [error, setError] = useState<string | null>(null);

  function resetForm() {
    setText("");
    setError(null);
  }

  function handleClose() {
    resetForm();
    onClose();
  }

  function handleContinue() {
    const items = parseUrlLinesToBatchItems(text);
    if (items.length === 0) {
      setError(t("download.batchAdd.inputEmpty"));
      return;
    }

    void openBatchAddPopup(
      { items, defaultQueueId },
      t("download.batchAdd.title"),
    );
    resetForm();
    onClose();
  }

  return (
    <Modal
      open={open}
      title={t("download.batchAdd.inputTitle")}
      onClose={handleClose}
      wide
      footer={
        <>
          <Button variant="secondary" onClick={handleClose}>
            {t("common.cancel")}
          </Button>
          <Button onClick={handleContinue}>{t("download.batchAdd.continue")}</Button>
        </>
      }
    >
      <label className="avar-field">
        <span className="avar-field__label">{t("download.batchAdd.inputLabel")}</span>
        <textarea
          className="avar-input avar-batch-add-input__textarea"
          rows={12}
          value={text}
          onChange={(e) => {
            setText(e.target.value);
            setError(null);
          }}
          placeholder={t("download.batchAdd.inputPlaceholder")}
          autoFocus
        />
        {error ? <span className="avar-field__error">{error}</span> : null}
        {!error ? (
          <span className="avar-field__hint">{t("download.batchAdd.inputHint")}</span>
        ) : null}
      </label>
    </Modal>
  );
}
