import type { ReactNode } from "react";

type BadgeTone = "default" | "success" | "warning" | "danger" | "info";

export interface BadgeProps {
  tone?: BadgeTone;
  children: ReactNode;
}

const toneClass: Record<BadgeTone, string> = {
  default: "avar-badge--default",
  success: "avar-badge--success",
  warning: "avar-badge--warning",
  danger: "avar-badge--danger",
  info: "avar-badge--info",
};

export function Badge({ tone = "default", children }: BadgeProps) {
  return <span className={`avar-badge ${toneClass[tone]}`}>{children}</span>;
}
