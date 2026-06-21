import { create } from "zustand";
import { persist } from "zustand/middleware";
import { GUI_CONFIG_KEY } from "@/config/defaults";
import { clampSize } from "@/hooks/useResize";

export type DownloadViewMode = "grid" | "compact";

export interface DownloadTableColumns {
  filename: number;
  status: number;
  progress: number;
  url: number;
}

export interface QueueTableColumns {
  name: number;
  description: number;
  status: number;
  downloads: number;
}

interface LayoutState {
  sidebarWidth: number;
  consoleHeight: number;
  detailPanelWidth: number;
  detailPanelOpen: boolean;
  downloadViewMode: DownloadViewMode;
  downloadTableColumns: DownloadTableColumns;
  queueTableColumns: QueueTableColumns;
  setSidebarWidth: (width: number) => void;
  adjustSidebarWidth: (delta: number) => void;
  setConsoleHeight: (height: number) => void;
  adjustConsoleHeight: (delta: number) => void;
  setDetailPanelWidth: (width: number) => void;
  adjustDetailPanelWidth: (delta: number) => void;
  setDetailPanelOpen: (open: boolean) => void;
  toggleDetailPanel: () => void;
  setDownloadViewMode: (mode: DownloadViewMode) => void;
  setDownloadTableColumn: (key: keyof DownloadTableColumns, width: number) => void;
  setQueueTableColumn: (key: keyof QueueTableColumns, width: number) => void;
}

const SIDEBAR_MIN = 180;
const SIDEBAR_MAX = 480;
const CONSOLE_MIN = 120;
const CONSOLE_MAX = 600;
const DETAIL_MIN = 220;
const DETAIL_MAX = 560;
const TABLE_COL_MIN = 60;

const defaultTableColumns: DownloadTableColumns = {
  filename: 200,
  status: 100,
  progress: 140,
  url: 180,
};

const defaultQueueTableColumns: QueueTableColumns = {
  name: 160,
  description: 200,
  status: 100,
  downloads: 100,
};

export const useLayoutStore = create<LayoutState>()(
  persist(
    (set, get) => ({
      sidebarWidth: 260,
      consoleHeight: 220,
      detailPanelWidth: 300,
      detailPanelOpen: false,
      downloadViewMode: "grid",
      downloadTableColumns: defaultTableColumns,
      queueTableColumns: defaultQueueTableColumns,

      setSidebarWidth: (width) =>
        set({ sidebarWidth: clampSize(width, SIDEBAR_MIN, SIDEBAR_MAX) }),

      adjustSidebarWidth: (delta) => {
        const next = get().sidebarWidth + delta;
        set({ sidebarWidth: clampSize(next, SIDEBAR_MIN, SIDEBAR_MAX) });
      },

      setConsoleHeight: (height) =>
        set({ consoleHeight: clampSize(height, CONSOLE_MIN, CONSOLE_MAX) }),

      adjustConsoleHeight: (delta) => {
        const next = get().consoleHeight - delta;
        set({ consoleHeight: clampSize(next, CONSOLE_MIN, CONSOLE_MAX) });
      },

      setDetailPanelWidth: (width) =>
        set({ detailPanelWidth: clampSize(width, DETAIL_MIN, DETAIL_MAX) }),

      adjustDetailPanelWidth: (delta) => {
        const next = get().detailPanelWidth - delta;
        set({ detailPanelWidth: clampSize(next, DETAIL_MIN, DETAIL_MAX) });
      },

      setDetailPanelOpen: (open) => set({ detailPanelOpen: open }),

      toggleDetailPanel: () => set((s) => ({ detailPanelOpen: !s.detailPanelOpen })),

      setDownloadViewMode: (mode) => set({ downloadViewMode: mode }),

      setDownloadTableColumn: (key, width) =>
        set((s) => ({
          downloadTableColumns: {
            ...s.downloadTableColumns,
            [key]: clampSize(width, TABLE_COL_MIN, 800),
          },
        })),

      setQueueTableColumn: (key, width) =>
        set((s) => ({
          queueTableColumns: {
            ...s.queueTableColumns,
            [key]: clampSize(width, TABLE_COL_MIN, 800),
          },
        })),
    }),
    {
      name: `${GUI_CONFIG_KEY}.layout`,
      partialize: (state) => ({
        sidebarWidth: state.sidebarWidth,
        consoleHeight: state.consoleHeight,
        detailPanelWidth: state.detailPanelWidth,
        detailPanelOpen: state.detailPanelOpen,
        downloadViewMode: state.downloadViewMode,
        downloadTableColumns: state.downloadTableColumns,
        queueTableColumns: state.queueTableColumns,
      }),
    },
  ),
);
