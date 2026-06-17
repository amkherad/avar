import type { ReactNode } from "react";

export interface TruncateWithTooltipProps {
  text: string;
  className?: string;
  children?: ReactNode;
}

/** Ellipsis text with a native tooltip showing the full value on hover. */
export function TruncateWithTooltip({
  text,
  className = "",
  children,
}: TruncateWithTooltipProps) {
  return (
    <span className={className} title={text}>
      {children ?? text}
    </span>
  );
}
