import type { InputHTMLAttributes } from "react";

export interface InputProps extends InputHTMLAttributes<HTMLInputElement> {
  label?: string;
  hint?: string;
  error?: string;
}

export function Input({ label, hint, error, id, className = "", ...rest }: InputProps) {
  const inputId = id ?? (label ? `input-${label.replace(/\s+/g, "-").toLowerCase()}` : undefined);

  return (
    <label className={`avar-field ${className}`.trim()} htmlFor={inputId}>
      {label ? <span className="avar-field__label">{label}</span> : null}
      <input id={inputId} className={`avar-input ${error ? "avar-input--error" : ""}`} {...rest} />
      {error ? <span className="avar-field__error">{error}</span> : null}
      {!error && hint ? <span className="avar-field__hint">{hint}</span> : null}
    </label>
  );
}
