import { create } from "zustand";
import { persist } from "zustand/middleware";
import { GUI_CONFIG_KEY } from "@/config/defaults";

export type LogSource = "gui" | "daemon";
export type LogLevel = "debug" | "info" | "warn" | "error";

export interface LogEntry {
  id: string;
  source: LogSource;
  level: LogLevel;
  message: string;
  detail?: string;
  timestamp: number;
}

export interface ConsoleSettings {
  maxEntries: number;
  autoScroll: boolean;
  showGuiLogs: boolean;
  showDaemonLogs: boolean;
  guiMinLevel: LogLevel;
  daemonMinLevel: LogLevel;
  daemonPollIntervalMs: number;
}

interface ConsoleState {
  open: boolean;
  entries: LogEntry[];
  settings: ConsoleSettings;
  setOpen: (open: boolean) => void;
  toggleOpen: () => void;
  append: (entry: Omit<LogEntry, "id">) => void;
  appendDaemonLines: (text: string) => void;
  clear: () => void;
  updateSettings: (patch: Partial<ConsoleSettings>) => void;
}

let entryCounter = 0;

function nextEntryId(): string {
  entryCounter += 1;
  return `log-${Date.now()}-${entryCounter}`;
}

const LEVEL_RANK: Record<LogLevel, number> = {
  debug: 0,
  info: 1,
  warn: 2,
  error: 3,
};

export function logLevelMeetsMin(level: LogLevel, min: LogLevel): boolean {
  return LEVEL_RANK[level] >= LEVEL_RANK[min];
}

function inferLevel(line: string): LogLevel {
  const lower = line.toLowerCase();
  if (lower.includes("error") || lower.includes("fatal") || lower.includes("fail")) {
    return "error";
  }
  if (lower.includes("warn")) {
    return "warn";
  }
  if (lower.includes("debug") || lower.includes("trace")) {
    return "debug";
  }
  return "info";
}

function pruneEntries(
  entries: LogEntry[],
  settings: ConsoleSettings,
): LogEntry[] {
  return entries.filter((entry) => {
    if (entry.source === "gui") {
      if (!settings.showGuiLogs) {
        return false;
      }
      return logLevelMeetsMin(entry.level, settings.guiMinLevel);
    }
    if (entry.source === "daemon") {
      if (!settings.showDaemonLogs) {
        return false;
      }
      return logLevelMeetsMin(entry.level, settings.daemonMinLevel);
    }
    return true;
  });
}

const defaultSettings: ConsoleSettings = {
  maxEntries: 500,
  autoScroll: true,
  showGuiLogs: true,
  showDaemonLogs: true,
  guiMinLevel: "info",
  daemonMinLevel: "warn",
  daemonPollIntervalMs: 3000,
};

export const useConsoleStore = create<ConsoleState>()(
  persist(
    (set, get) => ({
      open: false,
      entries: [],
      settings: defaultSettings,

      setOpen: (open) => {
        if (!open) {
          set({ open: false, entries: [] });
          return;
        }
        set({ open: true });
      },

      toggleOpen: () => {
        const { open } = get();
        if (open) {
          set({ open: false, entries: [] });
        } else {
          set({ open: true });
        }
      },

      append: (entry) => {
        const { open, settings } = get();
        if (!open) {
          return;
        }

        const minLevel = entry.source === "gui" ? settings.guiMinLevel : settings.daemonMinLevel;
        const show =
          entry.source === "gui" ? settings.showGuiLogs : settings.showDaemonLogs;
        if (!show || !logLevelMeetsMin(entry.level, minLevel)) {
          return;
        }

        const id = nextEntryId();
        set((state) => {
          const max = state.settings.maxEntries;
          const next = [...state.entries, { ...entry, id }];
          if (next.length > max) {
            return { entries: next.slice(next.length - max) };
          }
          return { entries: next };
        });
      },

      appendDaemonLines: (text) => {
        const { open, settings } = get();
        if (!open || !settings.showDaemonLogs) {
          return;
        }

        const lines = text.split(/\r?\n/).filter((line) => line.trim().length > 0);
        if (lines.length === 0) {
          return;
        }

        const additions: LogEntry[] = [];
        for (const line of lines) {
          const level = inferLevel(line);
          if (!logLevelMeetsMin(level, settings.daemonMinLevel)) {
            continue;
          }
          additions.push({
            id: nextEntryId(),
            source: "daemon",
            level,
            message: line,
            timestamp: Date.now(),
          });
        }

        if (additions.length === 0) {
          return;
        }

        set((state) => {
          const max = state.settings.maxEntries;
          const next = [...state.entries, ...additions];
          if (next.length > max) {
            return { entries: next.slice(next.length - max) };
          }
          return { entries: next };
        });
      },

      clear: () => set({ entries: [] }),

      updateSettings: (patch) =>
        set((state) => {
          const settings = { ...state.settings, ...patch };
          const severityTightened =
            (patch.guiMinLevel !== undefined &&
              LEVEL_RANK[patch.guiMinLevel] > LEVEL_RANK[state.settings.guiMinLevel]) ||
            (patch.daemonMinLevel !== undefined &&
              LEVEL_RANK[patch.daemonMinLevel] > LEVEL_RANK[state.settings.daemonMinLevel]);

          const entries =
            severityTightened || patch.showGuiLogs === false || patch.showDaemonLogs === false
              ? pruneEntries(state.entries, settings)
              : state.entries;

          return { settings, entries };
        }),
    }),
    {
      name: `${GUI_CONFIG_KEY}.console`,
      partialize: (state) => ({
        settings: state.settings,
      }),
      merge: (persisted, current) => {
        const stored = persisted as Partial<{ settings: Partial<ConsoleSettings> }> | undefined;
        if (!stored?.settings) {
          return current;
        }
        return {
          ...current,
          settings: { ...defaultSettings, ...stored.settings },
        };
      },
    },
  ),
);
