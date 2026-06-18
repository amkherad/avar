import { useEffect } from "react";
import { useConfigStore } from "@/stores/configStore";
import {
  isEditableTarget,
  normalizeCombo,
  type ShortcutActionId,
} from "./definitions";
import { useShortcutRegistry } from "./registry";

function findActionForCombo(
  combo: string,
  shortcuts: Record<ShortcutActionId, string>,
): ShortcutActionId | null {
  for (const [id, binding] of Object.entries(shortcuts) as [ShortcutActionId, string][]) {
    if (binding === combo) {
      return id;
    }
  }
  return null;
}

export function ShortcutProvider({ children }: { children: React.ReactNode }) {
  const shortcuts = useConfigStore((s) => s.config.shortcuts);
  const invoke = useShortcutRegistry((s) => s.invoke);

  useEffect(() => {
    function onKeyDown(event: KeyboardEvent) {
      if (event.defaultPrevented || isEditableTarget(event.target)) {
        return;
      }

      const combo = normalizeCombo(event);
      if (!combo) {
        return;
      }

      const actionId = findActionForCombo(combo, shortcuts);
      if (!actionId) {
        return;
      }

      event.preventDefault();
      invoke(actionId);
    }

    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [shortcuts, invoke]);

  return children;
}
