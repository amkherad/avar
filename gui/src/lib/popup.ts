import type { DownloadInfo } from "@/api/types";
import { usePopupStore } from "@/stores/popupStore";

const POPUP_STORAGE_PREFIX = "avar.popup.download.";
const CONFIRM_STORAGE_PREFIX = "avar.popup.confirm.";

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
  checkboxLabel?: string;
  checkboxDefault?: boolean;
}

export interface ConfirmDialogResult {
  confirmed: boolean;
  checkboxChecked: boolean;
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

export function parsePopupHash(
  hash: string,
): { type: "download"; id: string } | { type: "confirm"; id: string } | null {
  const downloadMatch = /^#\/popup\/download\/(.+)$/.exec(hash);
  if (downloadMatch) {
    return { type: "download", id: decodeURIComponent(downloadMatch[1]) };
  }
  const confirmMatch = /^#\/popup\/confirm\/(.+)$/.exec(hash);
  if (confirmMatch) {
    return { type: "confirm", id: decodeURIComponent(confirmMatch[1]) };
  }
  return null;
}

export function stashConfirmForPopup(id: string, options: ConfirmDialogOptions): void {
  localStorage.setItem(`${CONFIRM_STORAGE_PREFIX}${id}`, JSON.stringify(options));
}

export function readStashedConfirm(id: string): ConfirmDialogOptions | null {
  const raw = localStorage.getItem(`${CONFIRM_STORAGE_PREFIX}${id}`);
  if (!raw) {
    return null;
  }
  try {
    return JSON.parse(raw) as ConfirmDialogOptions;
  } catch {
    return null;
  }
}

export function resolveConfirmInPopup(id: string, result: ConfirmDialogResult): void {
  localStorage.setItem(`${CONFIRM_STORAGE_PREFIX}${id}.result`, JSON.stringify(result));
  localStorage.removeItem(`${CONFIRM_STORAGE_PREFIX}${id}`);
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
 * - Electron: separate BrowserWindow (popup route)
 * - Web: in-app confirm dialog (PopupHost)
 */
export function showConfirmDialog(options: ConfirmDialogOptions): Promise<ConfirmDialogResult> {
  if (window.avar?.isElectron) {
    return new Promise((resolve) => {
      const id = nextPopupId();
      const resultKey = `${CONFIRM_STORAGE_PREFIX}${id}.result`;
      stashConfirmForPopup(id, options);

      const onStorage = (event: StorageEvent) => {
        if (event.key !== resultKey || !event.newValue) {
          return;
        }
        window.removeEventListener("storage", onStorage);
        try {
          resolve(JSON.parse(event.newValue) as ConfirmDialogResult);
        } catch {
          resolve({ confirmed: false, checkboxChecked: false });
        }
        localStorage.removeItem(resultKey);
      };

      window.addEventListener("storage", onStorage);
      const height = options.checkboxLabel ? 280 : 240;
      void window.avar.openPopup({
        url: buildPopupUrl(`#/popup/confirm/${encodeURIComponent(id)}`),
        title: options.title,
        width: 440,
        height,
      });
    });
  }

  return new Promise((resolve) => {
    const id = nextPopupId();
    usePopupStore.getState().setConfirm({
      id,
      title: options.title,
      message: options.message,
      confirmLabel: options.confirmLabel ?? "OK",
      cancelLabel: options.cancelLabel ?? "Cancel",
      checkboxLabel: options.checkboxLabel,
      checkboxDefault: options.checkboxDefault ?? false,
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

export function resolveConfirm(confirmed: boolean, checkboxChecked = false): void {
  const { confirm, setConfirm } = usePopupStore.getState();
  if (confirm) {
    setConfirm(null);
    confirm.resolve({ confirmed, checkboxChecked });
  }
}
