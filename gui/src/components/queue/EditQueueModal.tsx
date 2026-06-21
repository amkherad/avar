import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Modal } from "@/components/ui/Modal";
import { useConnectionStore } from "@/stores/connectionStore";

export interface EditQueueModalProps {
  queue: QueueInfo | null;
  open: boolean;
  onClose: () => void;
  onSaved: () => void;
}

export function EditQueueModal({ queue, open, onClose, onSaved }: EditQueueModalProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [description, setDescription] = useState("");
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (queue && open) {
      setDescription(queue.description ?? "");
      setError(null);
    }
  }, [queue, open]);

  async function handleSave() {
    if (!client || !queue) {
      return;
    }
    setSaving(true);
    setError(null);
    try {
      await client.editQueue(queue.id, {
        description: description.trim() || undefined,
      });
      onSaved();
      onClose();
    } catch (err) {
      setError(err instanceof Error ? err.message : t("common.error"));
    } finally {
      setSaving(false);
    }
  }

  return (
    <Modal
      open={open}
      title={t("queue.modify")}
      onClose={onClose}
      footer={
        <>
          <Button variant="secondary" onClick={onClose}>
            {t("common.cancel")}
          </Button>
          <Button loading={saving} onClick={() => void handleSave()}>
            {t("common.save")}
          </Button>
        </>
      }
    >
      <Input label={t("queue.name")} value={queue?.name ?? ""} readOnly disabled />
      <Input
        label={t("queue.description")}
        value={description}
        onChange={(e) => setDescription(e.target.value)}
      />
      {error ? <p className="avar-field__error">{error}</p> : null}
    </Modal>
  );
}
