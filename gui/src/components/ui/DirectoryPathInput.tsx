import { useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faFolderOpen } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { RemoteDirectoryBrowser } from "@/components/ui/RemoteDirectoryBrowser";
import type { DirectoryPathInputMode } from "@/hooks/useDirectoryPathMode";
import { validateDirectoryPath } from "@/lib/directoryPath";
import type { InputHTMLAttributes } from "react";

export interface DirectoryPathInputProps
  extends Omit<InputHTMLAttributes<HTMLInputElement>, "type" | "onChange" | "value"> {
  mode: DirectoryPathInputMode;
  value: string;
  onChange: (value: string) => void;
  label?: string;
  hint?: string;
}

export function DirectoryPathInput({
  mode,
  value,
  onChange,
  label,
  hint,
  disabled,
  className = "",
  id,
  ...rest
}: DirectoryPathInputProps) {
  const { t } = useTranslation();
  const [validationError, setValidationError] = useState<string | null>(null);
  const [browserOpen, setBrowserOpen] = useState(false);

  const inputId =
    id ?? (label ? `input-${label.replace(/\s+/g, "-").toLowerCase()}` : undefined);

  function validateAndSet(next: string) {
    onChange(next);
    if (!next.trim()) {
      setValidationError(null);
      return;
    }
    const result = validateDirectoryPath(next);
    setValidationError(result.ok ? null : result.message);
  }

  async function browseLocal() {
    if (!window.avar?.selectDirectory) {
      return;
    }
    const selected = await window.avar.selectDirectory({
      defaultPath: value.trim() || undefined,
      title: label ?? t("directoryPath.select"),
    });
    if (selected) {
      validateAndSet(selected);
    }
  }

  const showBrowse = mode === "local" || mode === "remote";
  const browseHandler =
    mode === "local"
      ? () => void browseLocal()
      : mode === "remote"
        ? () => setBrowserOpen(true)
        : undefined;

  return (
    <>
      <div className={`avar-field avar-directory-path-input ${className}`.trim()}>
        {label ? <span className="avar-field__label">{label}</span> : null}
        <div className="avar-directory-path-input__row">
          <input
            {...rest}
            id={inputId}
            className={`avar-input ${validationError ? "avar-input--error" : ""}`}
            disabled={disabled}
            value={value}
            onChange={(e) => validateAndSet(e.target.value)}
            onBlur={() => {
              if (value.trim()) {
                const result = validateDirectoryPath(value);
                setValidationError(result.ok ? null : result.message);
              }
            }}
          />
          {showBrowse ? (
            <Button
              type="button"
              size="md"
              variant="secondary"
              className="avar-directory-path-input__browse"
              disabled={disabled}
              onClick={browseHandler}
              title={t("directoryPath.browse")}
              aria-label={t("directoryPath.browse")}
            >
              <FontAwesomeIcon icon={faFolderOpen} />
              {t("directoryPath.browse")}
            </Button>
          ) : null}
        </div>
        {validationError ? (
          <span className="avar-field__error">{validationError}</span>
        ) : hint ? (
          <span className="avar-field__hint">{hint}</span>
        ) : null}
      </div>

      {mode === "remote" ? (
        <RemoteDirectoryBrowser
          open={browserOpen}
          initialPath={value}
          onClose={() => setBrowserOpen(false)}
          onSelect={(path) => validateAndSet(path)}
        />
      ) : null}
    </>
  );
}
