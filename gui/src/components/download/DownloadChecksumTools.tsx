import { useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faShieldHalved } from "@fortawesome/free-solid-svg-icons";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { CopyButton } from "@/components/ui/CopyButton";
import { Input } from "@/components/ui/Input";
import { Select } from "@/components/ui/Select";
import { CHECKSUM_ALGORITHMS, type ChecksumAlgorithm } from "@/api/checksumTypes";
import { useConnectionStore } from "@/stores/connectionStore";
import { appLogger } from "@/lib/appLogger";
import { isCompleted } from "@/lib/downloadStatus";

export interface DownloadChecksumToolsProps {
  downloadId: string;
  status: string;
}

export function DownloadChecksumTools({ downloadId, status }: DownloadChecksumToolsProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [algorithm, setAlgorithm] = useState<ChecksumAlgorithm>("sha256");
  const [expectedHash, setExpectedHash] = useState("");
  const [computedHash, setComputedHash] = useState<string | null>(null);
  const [validationState, setValidationState] = useState<"match" | "mismatch" | null>(null);
  const [computing, setComputing] = useState(false);
  const [error, setError] = useState<string | null>(null);

  if (!isCompleted(status)) {
    return null;
  }

  async function handleCompute() {
    if (!client) {
      setError(t("download.checksum.disconnected"));
      return;
    }

    setComputing(true);
    setError(null);
    setComputedHash(null);
    setValidationState(null);

    appLogger.gui.debug("Download checksum compute", `${algorithm} ${downloadId}`);
    try {
      const result = await client.computeDownloadChecksum(
        downloadId,
        algorithm,
        expectedHash.trim() || undefined,
      );
      if (result.exitCode !== 0 || !result.checksum) {
        setError(t("download.checksum.error"));
        return;
      }
      setComputedHash(result.checksum);
      if (expectedHash.trim()) {
        setValidationState(result.match ? "match" : "mismatch");
        appLogger.gui.debug(
          "Download checksum validation",
          result.match ? "match" : "mismatch",
        );
      } else {
        appLogger.gui.debug("Download checksum computed", result.checksum.slice(0, 16));
      }
    } catch {
      setError(t("download.checksum.error"));
    } finally {
      setComputing(false);
    }
  }

  return (
    <section className="avar-download-detail__tools" aria-labelledby="avar-download-checksum-title">
      <h4 id="avar-download-checksum-title" className="avar-download-detail__tools-title">
        {t("download.tools.title")}
      </h4>

      <div className="avar-download-checksum">
        <p className="avar-download-checksum__hint">{t("download.checksum.hint")}</p>

        <Select
          label={t("download.checksum.algorithm")}
          value={algorithm}
          onChange={(event) => setAlgorithm(event.target.value as ChecksumAlgorithm)}
        >
          {CHECKSUM_ALGORITHMS.map((value) => (
            <option key={value} value={value}>
              {t(`download.checksum.algorithms.${value}`)}
            </option>
          ))}
        </Select>

        <Input
          label={t("download.checksum.expected")}
          hint={t("download.checksum.expectedHint")}
          value={expectedHash}
          onChange={(event) => setExpectedHash(event.target.value)}
          placeholder={t("download.checksum.expectedPlaceholder")}
          spellCheck={false}
          autoComplete="off"
        />

        <Button
          size="sm"
          variant="secondary"
          loading={computing}
          onClick={() => void handleCompute()}
        >
          <FontAwesomeIcon icon={faShieldHalved} />
          {t("download.checksum.compute")}
        </Button>

        {error ? <p className="avar-field__error">{error}</p> : null}

        {computedHash ? (
          <div className="avar-download-checksum__result">
            <span className="avar-field__label">{t("download.checksum.computed")}</span>
            <div className="avar-download-checksum__hash-row">
              <code className="avar-download-checksum__hash">{computedHash}</code>
              <CopyButton text={computedHash} label={t("download.checksum.copy")} />
            </div>
            {validationState === "match" ? (
              <Badge tone="success">{t("download.checksum.match")}</Badge>
            ) : null}
            {validationState === "mismatch" ? (
              <Badge tone="danger">{t("download.checksum.mismatch")}</Badge>
            ) : null}
          </div>
        ) : null}
      </div>
    </section>
  );
}
