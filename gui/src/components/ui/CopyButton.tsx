import { useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faCheck, faCopy } from "@fortawesome/free-solid-svg-icons";
import { copyTextToClipboard } from "@/lib/curlCommand";

export interface CopyButtonProps {
  text: string;
  label: string;
  className?: string;
}

export function CopyButton({ text, label, className = "" }: CopyButtonProps) {
  const { t } = useTranslation();
  const [copied, setCopied] = useState(false);

  async function handleCopy() {
    const ok = await copyTextToClipboard(text);
    if (ok) {
      setCopied(true);
      window.setTimeout(() => setCopied(false), 1500);
    }
  }

  return (
    <button
      type="button"
      className={`avar-copy-btn ${copied ? "avar-copy-btn--copied" : ""} ${className}`.trim()}
      aria-label={label}
      title={copied ? t("common.copied") : label}
      onClick={() => void handleCopy()}
    >
      <FontAwesomeIcon icon={copied ? faCheck : faCopy} />
    </button>
  );
}
