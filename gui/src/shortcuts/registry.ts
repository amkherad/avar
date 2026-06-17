import { create } from "zustand";
import type { ShortcutActionId } from "./definitions";

type ShortcutHandler = () => void | boolean | Promise<void>;

interface ShortcutRegistryState {
  handlers: Partial<Record<ShortcutActionId, ShortcutHandler>>;
  register: (id: ShortcutActionId, handler: ShortcutHandler) => void;
  unregister: (id: ShortcutActionId) => void;
  invoke: (id: ShortcutActionId) => void;
}

export const useShortcutRegistry = create<ShortcutRegistryState>()((set, get) => ({
  handlers: {},

  register: (id, handler) =>
    set((state) => ({
      handlers: { ...state.handlers, [id]: handler },
    })),

  unregister: (id) =>
    set((state) => {
      const next = { ...state.handlers };
      delete next[id];
      return { handlers: next };
    }),

  invoke: (id) => {
    const handler = get().handlers[id];
    if (handler) {
      void handler();
    }
  },
}));
