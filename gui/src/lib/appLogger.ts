import { useConsoleStore, type LogLevel, type LogSource } from "@/stores/consoleStore";

function emit(source: LogSource, level: LogLevel, message: string, detail?: string) {
  useConsoleStore.getState().append({
    source,
    level,
    message,
    detail,
    timestamp: Date.now(),
  });
}

export const appLogger = {
  gui: {
    info: (message: string, detail?: string) => emit("gui", "info", message, detail),
    warn: (message: string, detail?: string) => emit("gui", "warn", message, detail),
    error: (message: string, detail?: string) => emit("gui", "error", message, detail),
    debug: (message: string, detail?: string) => emit("gui", "debug", message, detail),
  },
  daemon: {
    info: (message: string, detail?: string) => emit("daemon", "info", message, detail),
    warn: (message: string, detail?: string) => emit("daemon", "warn", message, detail),
    error: (message: string, detail?: string) => emit("daemon", "error", message, detail),
    debug: (message: string, detail?: string) => emit("daemon", "debug", message, detail),
  },
};
