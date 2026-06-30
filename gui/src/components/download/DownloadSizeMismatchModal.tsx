import { useTranslation } from "react-i18next";
import type { DownloadInfo } from "@/api/types";
import { Modal } from "@/components/ui/Modal";
import { Button } from "@/components/ui/Button";
import { useUnitFormat } from "@/hooks/useUnitFormat";

export interface DownloadSizeMismatchModalProps {
  download: DownloadInfo;
  persistedBytes: number;
  probedBytes: number;
  busy?: boolean;
  onRestart: () => void;
  onNewDownload: () => void;
  onCancel: () => void;
}

export function DownloadSizeMismatchModal({
  download,
  persistedBytes,
  probedBytes,
  busy = false,
  onRestart,
  onNewDownload,
  onCancel,
}: DownloadSizeMismatchModalProps) {
  const { t } = useTranslation();
  const { formatBytes } = useUnitFormat();

  return (
    <Modal
      open
      title={t("download.sizeMismatchTitle")}
      onClose={onCancel}
      footer={
        <>
          <Button variant="ghost" onClick={onCancel} disabled={busy}>
            {t("download.sizeMismatchCancel")}
          </Button>
          <Button variant="secondary" onClick={() => void onNewDownload()} disabled={busy}>
            {t("download.sizeMismatchNew")}
          </Button>
          <Button onClick={() => void onRestart()} disabled={busy}>
            {t("download.sizeMismatchRestart")}
          </Button>
        </>
      }
    >
      <p>
        {t("download.sizeMismatchMessage", {
          name: download.filename,
          persisted: formatBytes(persistedBytes),
          probed: formatBytes(probedBytes),
        })}
      </p>
    </Modal>
  );
}
