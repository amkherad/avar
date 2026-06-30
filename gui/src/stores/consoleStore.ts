import { create } from "zustand";
import { persist } from "zustand/middleware";
import { GUI_CONFIG_KEY } from "@/config/defaults";
import { appLogger } from "@/lib/appLogger";

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
  hasUnseenErrors: boolean;
  daemonLogOffset: number;
  daemonLogEpoch: number;
  settings: ConsoleSettings;
  setOpen: (open: boolean) => void;
  toggleOpen: () => void;
  append: (entry: Omit<LogEntry, "id">) => void;
  appendDaemonLines: (text: string, epoch: number) => void;
  setDaemonLogOffset: (offset: number) => void;
  clear: () => number;
  markErrorsSeen: () => void;
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
  if (/\[(DBG|DEBUG)\]/i.test(line)) {
    return "debug";
  }
  if (/\[(WAR|WARN)\]/i.test(line)) {
    return "warn";
  }
  if (/\[(ERR|FTL|FATAL)\]/i.test(line) || /\berror\b/i.test(line) || /\bfail/i.test(line)) {
    return "error";
  }
  if (/\[(INF|INFO)\]/i.test(line)) {
    return "info";
  }
  const lower = line.toLowerCase();
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

function entryVisible(entry: LogEntry, settings: ConsoleSettings): boolean {
  if (entry.source === "gui") {
    return settings.showGuiLogs && logLevelMeetsMin(entry.level, settings.guiMinLevel);
  }
  return settings.showDaemonLogs && logLevelMeetsMin(entry.level, settings.daemonMinLevel);
}

const defaultSettings: ConsoleSettings = {
  maxEntries: 500,
  autoScroll: true,
  showGuiLogs: true,
  showDaemonLogs: true,
  guiMinLevel: "info",
  daemonMinLevel: "info",
  daemonPollIntervalMs: 3000,
};

export const useConsoleStore = create<ConsoleState>()(
  persist(
    (set, get) => ({
      open: false,
      entries: [],
      hasUnseenErrors: false,
      daemonLogOffset: 0,
      daemonLogEpoch: 0,
      settings: defaultSettings,

      setOpen: (open) => {
        appLogger.gui.debug("Console", open ? "opened" : "closed");
        if (open) {
          set({ open: true, hasUnseenErrors: false });
          return;
        }
        set({ open: false });
      },

      toggleOpen: () => {
        const { open } = get();
        appLogger.gui.debug("Console", open ? "closed" : "opened");
        if (open) {
          set({ open: false });
        } else {
          set({ open: true, hasUnseenErrors: false });
        }
      },

      markErrorsSeen: () => set({ hasUnseenErrors: false }),

      append: (entry) => {
        const { open, settings } = get();
        const id = nextEntryId();
        const fullEntry = { ...entry, id };

        set((state) => {
          const max = state.settings.maxEntries;
          const next = [...state.entries, fullEntry];
          const entries = next.length > max ? next.slice(next.length - max) : next;
          const visible = entryVisible(fullEntry, settings);
          const hasUnseenErrors =
            !open && visible && entry.level === "error"
              ? true
              : state.hasUnseenErrors;
          return { entries, hasUnseenErrors };
        });
      },

      appendDaemonLines: (text, epoch) => {
        const { open, settings, daemonLogEpoch } = get();
        if (!settings.showDaemonLogs || epoch !== daemonLogEpoch) {
          return;
        }

        const lines = text.split(/\r?\n/).filter((line) => line.trim().length > 0);
        if (lines.length === 0) {
          return;
        }

        const additions: LogEntry[] = [];
        for (const line of lines) {
          const level = inferLevel(line);
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
          if (state.daemonLogEpoch !== epoch) {
            return state;
          }
          const max = state.settings.maxEntries;
          const next = [...state.entries, ...additions];
          const entries = next.length > max ? next.slice(next.length - max) : next;
          const hasError = additions.some((item) => item.level === "error");
          const hasUnseenErrors = !open && hasError ? true : state.hasUnseenErrors;
          return { entries, hasUnseenErrors };
        });
      },

      clear: () => {
        appLogger.gui.debug("Console cleared");
        const nextEpoch = get().daemonLogEpoch + 1;
        set({ entries: [], hasUnseenErrors: false, daemonLogEpoch: nextEpoch });
        return nextEpoch;
      },

      setDaemonLogOffset: (offset) => set({ daemonLogOffset: offset }),

      updateSettings: (patch) => {
        const keys = Object.keys(patch);
        if (keys.length > 0) {
          appLogger.gui.debug("Console settings updated", keys.join(", "));
        }
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
        });
      },
    }),
    {
      name: `${GUI_CONFIG_KEY}.console`,
      partialize: (state) => ({
        settings: state.settings,
        daemonLogOffset: state.daemonLogOffset,
      }),
      merge: (persisted, current) => {
        const stored = persisted as
          | Partial<{ settings: Partial<ConsoleSettings>; daemonLogOffset?: number }>
          | undefined;
        if (!stored) {
          return current;
        }
        return {
          ...current,
          settings: stored.settings
            ? { ...defaultSettings, ...stored.settings }
            : current.settings,
          daemonLogOffset:
            typeof stored.daemonLogOffset === "number" && stored.daemonLogOffset >= 0
              ? stored.daemonLogOffset
              : current.daemonLogOffset,
        };
      },
    },
  ),
);
