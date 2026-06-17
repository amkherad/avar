import { create } from "zustand";

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
  resolve: (confirmed: boolean) => void;
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
