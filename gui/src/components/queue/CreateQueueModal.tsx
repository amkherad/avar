import { useState } from "react";
import { useTranslation } from "react-i18next";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Modal } from "@/components/ui/Modal";
import { useConnectionStore } from "@/stores/connectionStore";

export interface CreateQueueModalProps {
  open: boolean;
  onClose: () => void;
  onCreated: (id: string) => void;
}

export function CreateQueueModal({ open, onClose, onCreated }: CreateQueueModalProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [name, setName] = useState("");
  const [description, setDescription] = useState("");
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  function reset() {
    setName("");
    setDescription("");
    setError(null);
  }

  async function handleSave() {
    if (!client || !name.trim()) {
      return;
    }
    setSaving(true);
    setError(null);
    try {
      const id = await client.addQueue({
        name: name.trim(),
        description: description.trim() || undefined,
      });
      reset();
      onCreated(id);
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
      title={t("queue.add")}
      onClose={() => {
        reset();
        onClose();
      }}
      footer={
        <>
          <Button
            variant="secondary"
            onClick={() => {
              reset();
              onClose();
            }}
          >
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
