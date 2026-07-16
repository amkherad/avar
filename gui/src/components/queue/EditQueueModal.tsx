import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import type { QueueInfo } from "@/api/types";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Modal } from "@/components/ui/Modal";
import { useConnectionStore } from "@/stores/connectionStore";
import { appLogger } from "@/lib/appLogger";

export interface EditQueueModalProps {
  queue: QueueInfo | null;
  open: boolean;
  onClose: () => void;
  onSaved: () => void;
}

export function EditQueueModal({ queue, open, onClose, onSaved }: EditQueueModalProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [name, setName] = useState("");
  const [description, setDescription] = useState("");
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (queue && open) {
      setName(queue.name);
      setDescription(queue.description ?? "");
      setError(null);
    }
  }, [queue, open]);

  async function handleSave() {
    if (!client || !queue || !name.trim()) {
      return;
    }

    const trimmedName = name.trim();
    const trimmedDescription = description.trim();
    const patch: { name?: string; description?: string } = {};

    if (trimmedName !== queue.name) {
      patch.name = trimmedName;
    }
    if (trimmedDescription !== (queue.description ?? "")) {
      patch.description = trimmedDescription || undefined;
    }

    if (Object.keys(patch).length === 0) {
      onClose();
      return;
    }

    setSaving(true);
    setError(null);
    try {
      await client.editQueue(queue.id, patch);
      appLogger.gui.info("Queue updated", queue.id);
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
      title={t("queue.rename")}
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
      <Input
        label={t("queue.name")}
        value={name}
        onChange={(e) => setName(e.target.value)}
        autoFocus
      />
      <Input
        label={t("queue.description")}
        value={description}
        onChange={(e) => setDescription(e.target.value)}
      />
      {error ? <p className="avar-field__error">{error}</p> : null}
    </Modal>
  );
}
