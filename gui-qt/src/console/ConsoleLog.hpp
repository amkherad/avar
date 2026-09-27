#pragma once

#include <QString>

namespace avar::gui {

enum class ConsoleLogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

enum class ConsoleLogSource {
    Gui,
    Daemon,
};

[[nodiscard]] int consoleLogLevelRank(ConsoleLogLevel level);
[[nodiscard]] bool logLevelMeetsMin(ConsoleLogLevel level, ConsoleLogLevel min);
[[nodiscard]] ConsoleLogLevel inferDaemonLogLevel(const QString &line);
[[nodiscard]] ConsoleLogLevel consoleLogLevelFromSettings(const QString &value);
[[nodiscard]] QString consoleLogLevelToSettings(ConsoleLogLevel level);
[[nodiscard]] QString consoleLogSourceLabel(ConsoleLogSource source);
[[nodiscard]] QString consoleLogLevelLabel(ConsoleLogLevel level);

/** Parses lines emitted by GuiLog (`[iso] [gui] [level] message`). */
[[nodiscard]] bool parseGuiLogLine(const QString &line,
                                   qint64 *timestampMs,
                                   ConsoleLogLevel *level,
                                   QString *message);

} // namespace avar::gui
