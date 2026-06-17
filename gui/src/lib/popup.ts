import type { DownloadInfo } from "@/api/types";
import { usePopupStore } from "@/stores/popupStore";

const POPUP_STORAGE_PREFIX = "avar.popup.download.";

export interface PopupWindowOptions {
  width?: number;
  height?: number;
  title?: string;
}

export interface ConfirmDialogOptions {
  title: string;
  message: string;
  confirmLabel?: string;
  cancelLabel?: string;
}

function nextPopupId(): string {
  return `popup-${Date.now()}-${Math.random().toString(36).slice(2, 9)}`;
}

function buildPopupUrl(hash: string): string {
  return `${window.location.origin}${window.location.pathname}${window.location.search}${hash}`;
}

export function stashDownloadForPopup(download: DownloadInfo): void {
  if (!download.id) {
    return;
  }
  const key = `${POPUP_STORAGE_PREFIX}${download.id}`;
  const payload = JSON.stringify(download);
  sessionStorage.setItem(key, payload);
  localStorage.setItem(key, payload);
}

export function readStashedDownload(id: string): DownloadInfo | null {
  const raw =
    sessionStorage.getItem(`${POPUP_STORAGE_PREFIX}${id}`) ??
    localStorage.getItem(`${POPUP_STORAGE_PREFIX}${id}`);
  if (!raw) {
    return null;
  }
  try {
    return JSON.parse(raw) as DownloadInfo;
  } catch {
    return null;
  }
}

export function parsePopupHash(hash: string): { type: "download"; id: string } | null {
  const match = /^#\/popup\/download\/(.+)$/.exec(hash);
  if (!match) {
    return null;
  }
  return { type: "download", id: decodeURIComponent(match[1]) };
}

/**
 * Opens a popup window. Resolves when the window is closed.
 * - Electron: native OS window
 * - Web: in-app floating window (see PopupHost)
 */
export function openPopupWindow(
  hash: string,
  options: PopupWindowOptions = {},
): Promise<void> {
  const width = options.width ?? 520;
  const height = options.height ?? 640;
  const title = options.title ?? "Avar";
  const url = buildPopupUrl(hash);

  if (window.avar?.isElectron) {
    void window.avar.openPopup({ url, title, width, height });
    return Promise.resolve();
  }

  return new Promise((resolve) => {
    const id = nextPopupId();
    usePopupStore.getState().addWindow({
      id,
      title,
      url,
      width,
      height,
      resolve,
    });
  });
}

export function openDownloadPopup(
  download: DownloadInfo,
  title: string,
  options: PopupWindowOptions = {},
): Promise<void> {
  if (!download.id) {
    return Promise.resolve();
  }

  stashDownloadForPopup(download);
  const hash = `#/popup/download/${encodeURIComponent(download.id)}`;
  return openPopupWindow(hash, { ...options, title });
}

/**
 * Shows a confirmation dialog. Resolves true if confirmed, false if cancelled.
 * - Electron: in-app confirm (PopupHost) — OS confirm is not used for consistency
 * - Web: in-app confirm dialog (PopupHost)
 */
export function showConfirmDialog(options: ConfirmDialogOptions): Promise<boolean> {
  return new Promise((resolve) => {
    const id = nextPopupId();
    usePopupStore.getState().setConfirm({
      id,
      title: options.title,
      message: options.message,
      confirmLabel: options.confirmLabel ?? "OK",
      cancelLabel: options.cancelLabel ?? "Cancel",
      resolve,
    });
  });
}

export function closePopupWindow(id: string): void {
  const { windows, removeWindow } = usePopupStore.getState();
  const win = windows.find((w) => w.id === id);
  if (win) {
    removeWindow(id);
    win.resolve();
  }
}

export function resolveConfirm(confirmed: boolean): void {
  const { confirm, setConfirm } = usePopupStore.getState();
  if (confirm) {
    setConfirm(null);
    confirm.resolve(confirmed);
  }
}
