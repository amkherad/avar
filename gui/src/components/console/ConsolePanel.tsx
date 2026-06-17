import { useEffect, useRef } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faXmark } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { Select } from "@/components/ui/Select";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import { useConnectionStore } from "@/stores/connectionStore";
import { useConsoleStore, type LogEntry, type LogLevel } from "@/stores/consoleStore";
import { useLayoutStore } from "@/stores/layoutStore";
import { appLogger } from "@/lib/appLogger";

const LOG_LEVELS: LogLevel[] = ["debug", "info", "warn", "error"];

function formatTime(ts: number): string {
  const d = new Date(ts);
  return d.toLocaleTimeString(undefined, { hour12: false });
}

function levelClass(level: LogEntry["level"]): string {
  return `avar-console__line--${level}`;
}

function sourceClass(source: LogEntry["source"]): string {
  return `avar-console__line--${source}`;
}

export function ConsolePanel() {
  const { t } = useTranslation();
  const open = useConsoleStore((s) => s.open);
  const entries = useConsoleStore((s) => s.entries);
  const settings = useConsoleStore((s) => s.settings);
  const setOpen = useConsoleStore((s) => s.setOpen);
  const clear = useConsoleStore((s) => s.clear);
  const updateSettings = useConsoleStore((s) => s.updateSettings);
  const appendDaemonLines = useConsoleStore((s) => s.appendDaemonLines);
  const consoleHeight = useLayoutStore((s) => s.consoleHeight);
  const adjustConsoleHeight = useLayoutStore((s) => s.adjustConsoleHeight);

  const connection = useConnectionStore((s) => s.connection);
  const client = useConnectionStore((s) => s.client);
  const scrollRef = useRef<HTMLDivElement>(null);

  const visibleEntries = entries.filter((entry) => {
    if (entry.source === "gui" && !settings.showGuiLogs) {
      return false;
    }
    if (entry.source === "daemon" && !settings.showDaemonLogs) {
      return false;
    }
    return true;
  });

  useEffect(() => {
    if (!open || connection !== "connected" || !client || !settings.showDaemonLogs) {
      return;
    }

    let cancelled = false;

    async function poll() {
      try {
        appLogger.gui.debug("Polling daemon logs");
        const text = await client!.getLogs(80);
        if (!cancelled && text) {
          appendDaemonLines(text);
        }
      } catch (err) {
        if (!cancelled) {
          appLogger.gui.warn(
            t("console.daemonFetchFailed"),
            err instanceof Error ? err.message : undefined,
          );
        }
      }
    }

    void poll();
    const timer = window.setInterval(() => void poll(), settings.daemonPollIntervalMs);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [
    open,
    connection,
    client,
    settings.showDaemonLogs,
    settings.daemonPollIntervalMs,
    appendDaemonLines,
    t,
  ]);

  useEffect(() => {
    if (!open || !settings.autoScroll || !scrollRef.current) {
      return;
    }
    scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
  }, [open, settings.autoScroll, visibleEntries.length]);

  if (!open) {
    return null;
  }

  return (
    <>
      <ResizeHandle
        axis="vertical"
        min={120}
        max={600}
        label={t("console.resize")}
        onResize={adjustConsoleHeight}
      />
      <section
        className="avar-console"
        style={{ height: consoleHeight }}
        aria-label={t("console.title")}
      >
        <header className="avar-console__header">
          <h2 className="avar-console__title">{t("console.title")}</h2>
          <div className="avar-console__header-settings">
            <label className="avar-checkbox-row avar-console__inline-setting">
              <input
                type="checkbox"
                checked={settings.autoScroll}
                onChange={(e) => updateSettings({ autoScroll: e.target.checked })}
              />
              {t("console.autoScroll")}
            </label>
            <label className="avar-checkbox-row avar-console__inline-setting">
              <input
                type="checkbox"
                checked={settings.showGuiLogs}
                onChange={(e) => updateSettings({ showGuiLogs: e.target.checked })}
              />
              {t("console.showGui")}
            </label>
            <Select
              className="avar-console__severity-select"
              label={t("console.guiSeverity")}
              value={settings.guiMinLevel}
              onChange={(e) =>
                updateSettings({ guiMinLevel: e.target.value as LogLevel })
              }
            >
              {LOG_LEVELS.map((level) => (
                <option key={level} value={level}>
                  {t(`console.level.${level}`)}
                </option>
              ))}
            </Select>
            <label className="avar-checkbox-row avar-console__inline-setting">
              <input
                type="checkbox"
                checked={settings.showDaemonLogs}
                onChange={(e) => updateSettings({ showDaemonLogs: e.target.checked })}
              />
              {t("console.showDaemon")}
            </label>
            <Select
              className="avar-console__severity-select"
              label={t("console.daemonSeverity")}
              value={settings.daemonMinLevel}
              onChange={(e) =>
                updateSettings({ daemonMinLevel: e.target.value as LogLevel })
              }
            >
              {LOG_LEVELS.map((level) => (
                <option key={level} value={level}>
                  {t(`console.level.${level}`)}
                </option>
              ))}
            </Select>
          </div>
          <div className="avar-console__header-actions">
            <Button size="sm" variant="ghost" onClick={() => clear()}>
              {t("console.clear")}
            </Button>
            <Button
              size="sm"
              variant="ghost"
              aria-label={t("console.close")}
              onClick={() => setOpen(false)}
            >
              <FontAwesomeIcon icon={faXmark} />
            </Button>
          </div>
        </header>

        <div className="avar-console__output" ref={scrollRef}>
          {visibleEntries.length === 0 ? (
            <p className="avar-console__empty">{t("console.empty")}</p>
          ) : (
            visibleEntries.map((entry) => (
              <div
                key={entry.id}
                className={`avar-console__line ${levelClass(entry.level)} ${sourceClass(entry.source)}`}
              >
                <span className="avar-console__time">{formatTime(entry.timestamp)}</span>
                <span className="avar-console__source">[{entry.source}]</span>
                <span className="avar-console__level">{entry.level}</span>
                <span className="avar-console__message">{entry.message}</span>
                {entry.detail ? (
                  <span className="avar-console__detail">{entry.detail}</span>
                ) : null}
              </div>
            ))
          )}
        </div>
      </section>
    </>
  );
}
