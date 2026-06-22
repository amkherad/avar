import { useCallback, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faFolder, faFolderOpen } from "@fortawesome/free-solid-svg-icons";
import { Modal } from "@/components/ui/Modal";
import { Button } from "@/components/ui/Button";
import { Spinner } from "@/components/ui/Spinner";
import { useConnectionStore } from "@/stores/connectionStore";
import { joinDirectoryPath } from "@/lib/directoryPath";

export interface RemoteDirectoryBrowserProps {
  open: boolean;
  initialPath?: string;
  onClose: () => void;
  onSelect: (path: string) => void;
}

interface BrowseEntry {
  name: string;
}

export function RemoteDirectoryBrowser({
  open,
  initialPath = "",
  onClose,
  onSelect,
}: RemoteDirectoryBrowserProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [currentPath, setCurrentPath] = useState(initialPath);
  const [parentPath, setParentPath] = useState<string | null>(null);
  const [entries, setEntries] = useState<BrowseEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const loadPath = useCallback(
    async (path: string) => {
      if (!client) {
        setError(t("directoryPath.remoteDisconnected"));
        return;
      }

      setLoading(true);
      setError(null);
      try {
        const result = await client.browseDirectory(path);
        setCurrentPath(result.path);
        setParentPath(result.parent ?? null);
        setEntries(result.entries);
      } catch (err) {
        setError(err instanceof Error ? err.message : t("common.error"));
      } finally {
        setLoading(false);
      }
    },
    [client, t],
  );

  useEffect(() => {
    if (!open) {
      return;
    }
    void loadPath(initialPath);
  }, [open, initialPath, loadPath]);

  function handleSelectCurrent() {
    if (currentPath) {
      onSelect(currentPath);
      onClose();
    }
  }

  return (
    <Modal
      open={open}
      title={t("directoryPath.remoteTitle")}
      onClose={onClose}
      wide
      footer={
        <>
          <Button variant="secondary" onClick={onClose}>
            {t("common.cancel")}
          </Button>
          <Button onClick={handleSelectCurrent} disabled={!currentPath || loading}>
            {t("directoryPath.select")}
          </Button>
        </>
      }
    >
      <div className="avar-remote-dir-browser">
        <p className="avar-remote-dir-browser__path" title={currentPath}>
          {currentPath || t("directoryPath.remoteRoot")}
        </p>

        {loading ? <Spinner /> : null}
        {error ? <p className="avar-field__error">{error}</p> : null}

        {!loading && !error ? (
          <ul className="avar-remote-dir-browser__list">
            {parentPath ? (
              <li>
                <button
                  type="button"
                  className="avar-remote-dir-browser__item"
                  onClick={() => void loadPath(parentPath)}
                >
                  <FontAwesomeIcon icon={faFolderOpen} />
                  <span>..</span>
                </button>
              </li>
            ) : null}
            {entries.map((entry) => (
              <li key={entry.name}>
                <button
                  type="button"
                  className="avar-remote-dir-browser__item"
                  onClick={() => void loadPath(joinDirectoryPath(currentPath, entry.name))}
                >
                  <FontAwesomeIcon icon={faFolder} />
                  <span>{entry.name}</span>
                </button>
              </li>
            ))}
          </ul>
        ) : null}
      </div>
    </Modal>
  );
}
