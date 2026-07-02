import { useState } from "react";
import { useTranslation } from "react-i18next";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";
import { Modal } from "@/components/ui/Modal";
import { useConnectionStore } from "@/stores/connectionStore";
import { appLogger } from "@/lib/appLogger";

export interface AddBookmarkModalProps {
  open: boolean;
  onClose: () => void;
  onCreated: () => void;
}

function isHttpUrl(value: string): boolean {
  try {
    const parsed = new URL(value.trim());
    return parsed.protocol === "http:" || parsed.protocol === "https:";
  } catch {
    return false;
  }
}

export function AddBookmarkModal({ open, onClose, onCreated }: AddBookmarkModalProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [url, setUrl] = useState("");
  const [title, setTitle] = useState("");
  const [linkCount, setLinkCount] = useState("");
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  function reset() {
    setUrl("");
    setTitle("");
    setLinkCount("");
    setError(null);
  }

  async function handleSave() {
    if (!client) {
      return;
    }
    const trimmedUrl = url.trim();
    if (!trimmedUrl) {
      setError(t("bookmark.urlRequired"));
      return;
    }
    if (!isHttpUrl(trimmedUrl)) {
      setError(t("bookmark.urlInvalid"));
      return;
    }

    const parsedLinkCount = linkCount.trim() ? Number(linkCount) : 0;
    if (!Number.isFinite(parsedLinkCount) || parsedLinkCount < 0) {
      setError(t("bookmark.linkCountInvalid"));
      return;
    }

    setSaving(true);
    setError(null);
    try {
      await client.addBookmark({
        url: trimmedUrl,
        title: title.trim() || trimmedUrl,
        linkCount: Math.floor(parsedLinkCount),
      });
      appLogger.gui.info("Bookmark added", trimmedUrl);
      reset();
      onCreated();
      onClose();
    } catch (err) {
      setError(err instanceof Error ? err.message : t("bookmark.addFailed"));
    } finally {
      setSaving(false);
    }
  }

  return (
    <Modal
      open={open}
      title={t("bookmark.add")}
      onClose={() => {
        reset();
        onClose();
      }}
      footer={
        <>
          <Button
            variant="ghost"
            onClick={() => {
              reset();
              onClose();
            }}
          >
            {t("common.cancel")}
          </Button>
          <Button loading={saving} onClick={() => void handleSave()}>
            {t("bookmark.add")}
          </Button>
        </>
      }
    >
      <Input
        label={t("bookmark.url")}
        value={url}
        onChange={(e) => setUrl(e.target.value)}
        placeholder="https://example.com/page"
        autoFocus
      />
      <Input
        label={t("bookmark.pageTitle")}
        value={title}
        onChange={(e) => setTitle(e.target.value)}
        hint={t("bookmark.pageTitleHint")}
      />
      <Input
        label={t("bookmark.linkCountField")}
        type="number"
        min={0}
        value={linkCount}
        onChange={(e) => setLinkCount(e.target.value)}
        hint={t("bookmark.linkCountFieldHint")}
      />
      {error ? <p className="avar-field__error">{error}</p> : null}
    </Modal>
  );
}
