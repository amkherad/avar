import type { SelectHTMLAttributes, ReactNode } from "react";

export interface SelectProps extends SelectHTMLAttributes<HTMLSelectElement> {
  label?: string;
  children: ReactNode;
}

export function Select({ label, id, className = "", children, ...rest }: SelectProps) {
  const selectId = id ?? (label ? `select-${label.replace(/\s+/g, "-").toLowerCase()}` : undefined);

  return (
    <label className={`avar-field ${className}`.trim()} htmlFor={selectId}>
      {label ? <span className="avar-field__label">{label}</span> : null}
      <select id={selectId} className="avar-select" {...rest}>
        {children}
      </select>
    </label>
  );
}
