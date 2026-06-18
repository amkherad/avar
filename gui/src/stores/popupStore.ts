import { create } from "zustand";
import type { ConfirmDialogResult } from "@/lib/popup";

export interface PopupWindowState {
  id: string;
  title: string;
  url: string;
  width: number;
  height: number;
  resolve: () => void;
}

export interface ConfirmDialogState {
  id: string;
  title: string;
  message: string;
  confirmLabel: string;
  cancelLabel: string;
  checkboxLabel?: string;
  checkboxDefault?: boolean;
  resolve: (result: ConfirmDialogResult) => void;
}

interface PopupStoreState {
  windows: PopupWindowState[];
  confirm: ConfirmDialogState | null;
  addWindow: (window: PopupWindowState) => void;
  removeWindow: (id: string) => void;
  setConfirm: (dialog: ConfirmDialogState | null) => void;
}

export const usePopupStore = create<PopupStoreState>()((set) => ({
  windows: [],
  confirm: null,

  addWindow: (window) =>
    set((state) => ({ windows: [...state.windows, window] })),

  removeWindow: (id) =>
    set((state) => ({ windows: state.windows.filter((w) => w.id !== id) })),

  setConfirm: (confirm) => set({ confirm }),
}));
