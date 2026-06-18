import { useEffect } from "react";
import type { ShortcutActionId } from "./definitions";
import { useShortcutRegistry } from "./registry";

export function useShortcutAction(
  id: ShortcutActionId | undefined,
  handler: () => void | boolean | Promise<void>,
  enabled = true,
): void {
  const register = useShortcutRegistry((s) => s.register);
  const unregister = useShortcutRegistry((s) => s.unregister);

  useEffect(() => {
    if (!enabled || !id) {
      return;
    }
    register(id, handler);
    return () => unregister(id);
  }, [id, handler, enabled, register, unregister]);
}
