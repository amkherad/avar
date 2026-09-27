#include "console/ConsoleLog.hpp"

#include <QDateTime>
#include <QRegularExpression>

namespace avar::gui {

int consoleLogLevelRank(ConsoleLogLevel level)
{
    switch (level) {
    case ConsoleLogLevel::Debug:
        return 0;
    case ConsoleLogLevel::Info:
        return 1;
    case ConsoleLogLevel::Warn:
        return 2;
    case ConsoleLogLevel::Error:
        return 3;
    }
    return 1;
}

bool logLevelMeetsMin(ConsoleLogLevel level, ConsoleLogLevel min)
{
    return consoleLogLevelRank(level) >= consoleLogLevelRank(min);
}

ConsoleLogLevel inferDaemonLogLevel(const QString &line)
{
    static const QRegularExpression dbg(QStringLiteral(R"(\[(DBG|DEBUG)\])"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression war(QStringLiteral(R"(\[(WAR|WARN)\])"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression err(QStringLiteral(R"(\[(ERR|FTL|FATAL)\])"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression inf(QStringLiteral(R"(\[(INF|INFO)\])"), QRegularExpression::CaseInsensitiveOption);

    if (dbg.match(line).hasMatch()) {
        return ConsoleLogLevel::Debug;
    }
    if (war.match(line).hasMatch()) {
        return ConsoleLogLevel::Warn;
    }
    if (err.match(line).hasMatch() || line.contains(QStringLiteral("error"), Qt::CaseInsensitive)
        || line.contains(QStringLiteral("fail"), Qt::CaseInsensitive)) {
        return ConsoleLogLevel::Error;
    }
    if (inf.match(line).hasMatch()) {
        return ConsoleLogLevel::Info;
    }
    const QString lower = line.toLower();
    if (lower.contains(QStringLiteral("warn"))) {
        return ConsoleLogLevel::Warn;
    }
    if (lower.contains(QStringLiteral("debug")) || lower.contains(QStringLiteral("trace"))) {
        return ConsoleLogLevel::Debug;
    }
    return ConsoleLogLevel::Info;
}

ConsoleLogLevel consoleLogLevelFromSettings(const QString &value)
{
    if (value == QStringLiteral("debug")) {
        return ConsoleLogLevel::Debug;
    }
    if (value == QStringLiteral("warn")) {
        return ConsoleLogLevel::Warn;
    }
    if (value == QStringLiteral("error")) {
        return ConsoleLogLevel::Error;
    }
    return ConsoleLogLevel::Info;
}

QString consoleLogLevelToSettings(ConsoleLogLevel level)
{
    switch (level) {
    case ConsoleLogLevel::Debug:
        return QStringLiteral("debug");
    case ConsoleLogLevel::Info:
        return QStringLiteral("info");
    case ConsoleLogLevel::Warn:
        return QStringLiteral("warn");
    case ConsoleLogLevel::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("info");
}

QString consoleLogSourceLabel(ConsoleLogSource source)
{
    return source == ConsoleLogSource::Gui ? QStringLiteral("gui") : QStringLiteral("daemon");
}

QString consoleLogLevelLabel(ConsoleLogLevel level)
{
    return consoleLogLevelToSettings(level);
}

bool parseGuiLogLine(const QString &line, qint64 *timestampMs, ConsoleLogLevel *level, QString *message)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^\[(.+?)\] \[gui\] \[(debug|info|warn|error)\] (.*)$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(line);
    if (!match.hasMatch()) {
        return false;
    }
    const QDateTime dt = QDateTime::fromString(match.captured(1), Qt::ISODate);
    if (timestampMs) {
        *timestampMs = dt.isValid() ? dt.toMSecsSinceEpoch() : QDateTime::currentMSecsSinceEpoch();
    }
    if (level) {
        *level = consoleLogLevelFromSettings(match.captured(2).toLower());
    }
    if (message) {
        *message = match.captured(3);
    }
    return true;
}

} // namespace avar::gui
