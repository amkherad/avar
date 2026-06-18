import type { ButtonHTMLAttributes } from "react";
import { useConfigStore } from "@/stores/configStore";
import { formatCombo, SHORTCUT_DEFINITIONS, type ShortcutActionId } from "@/shortcuts/definitions";
import { useShortcutAction } from "@/shortcuts/useShortcutAction";
import { Button, type ButtonProps } from "@/components/ui/Button";
import { useTranslation } from "react-i18next";

export interface ShortcutButtonProps extends ButtonProps {
  shortcut?: ShortcutActionId;
  shortcutEnabled?: boolean;
  registerShortcut?: boolean;
}

export function ShortcutButton({
  shortcut,
  shortcutEnabled = true,
  registerShortcut = true,
  title,
  onClick,
  ...rest
}: ShortcutButtonProps) {
  const { t } = useTranslation();
  const combo = useConfigStore((s) =>
    shortcut ? s.config.shortcuts[shortcut] : undefined,
  );

  useShortcutAction(
    shortcut,
    () => {
      if (onClick) {
        onClick({} as ButtonHTMLAttributes<HTMLButtonElement> as never);
      }
    },
    registerShortcut && shortcutEnabled && Boolean(shortcut && onClick),
  );

  const def = shortcut
    ? SHORTCUT_DEFINITIONS.find((item) => item.id === shortcut)
    : undefined;
  const label = def ? t(def.labelKey) : "";
  const resolvedTitle =
    title ?? (shortcut && combo ? `${label} (${formatCombo(combo)})` : label || undefined);

  return <Button title={resolvedTitle} onClick={onClick} {...rest} />;
}
