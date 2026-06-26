import { useConsoleStore, type LogLevel, type LogSource } from "@/stores/consoleStore";

function normalizeDetail(detail?: unknown): string | undefined {
  if (detail === undefined) {
    return undefined;
  }
  if (typeof detail === "string") {
    return detail;
  }
  if (detail instanceof Error) {
    return detail.message;
  }
  return String(detail);
}

function emit(source: LogSource, level: LogLevel, message: string, detail?: unknown) {
  useConsoleStore.getState().append({
    source,
    level,
    message,
    detail: normalizeDetail(detail),
    timestamp: Date.now(),
  });
}

export const appLogger = {
  gui: {
    info: (message: string, detail?: unknown) => emit("gui", "info", message, detail),
    warn: (message: string, detail?: unknown) => emit("gui", "warn", message, detail),
    error: (message: string, detail?: unknown) => emit("gui", "error", message, detail),
    debug: (message: string, detail?: unknown) => emit("gui", "debug", message, detail),
  },
  daemon: {
    info: (message: string, detail?: unknown) => emit("daemon", "info", message, detail),
    warn: (message: string, detail?: unknown) => emit("daemon", "warn", message, detail),
    error: (message: string, detail?: unknown) => emit("daemon", "error", message, detail),
    debug: (message: string, detail?: unknown) => emit("daemon", "debug", message, detail),
  },
};
