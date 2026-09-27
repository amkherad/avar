#include "console/ConsoleStore.hpp"

#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

namespace avar::gui {

namespace {

void loadSettings(ConsoleSettings &out)
{
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    store.beginGroup(QStringLiteral("console"));
    out.maxEntries = store.value(QStringLiteral("maxEntries"), 500).toInt();
    out.autoScroll = store.value(QStringLiteral("autoScroll"), true).toBool();
    out.showGuiLogs = store.value(QStringLiteral("showGuiLogs"), true).toBool();
    out.showDaemonLogs = store.value(QStringLiteral("showDaemonLogs"), true).toBool();
    out.guiMinLevel =
        consoleLogLevelFromSettings(store.value(QStringLiteral("guiMinLevel"), QStringLiteral("info")).toString());
    out.daemonMinLevel =
        consoleLogLevelFromSettings(store.value(QStringLiteral("daemonMinLevel"), QStringLiteral("info")).toString());
    out.daemonPollIntervalMs = store.value(QStringLiteral("daemonPollIntervalMs"), 3000).toInt();
    store.endGroup();
}
} // namespace

ConsoleStore::ConsoleStore(QObject *parent)
    : QObject(parent)
{
    loadSettings(m_settings);
}

const QVector<ConsoleLogEntry> &ConsoleStore::entries() const
{
    return m_entries;
}

ConsoleSettings ConsoleStore::settings() const
{
    return m_settings;
}

bool ConsoleStore::hasUnseenErrors() const
{
    return m_hasUnseenErrors;
}

qint64 ConsoleStore::daemonLogOffset() const
{
    return m_daemonLogOffset;
}

int ConsoleStore::daemonLogEpoch() const
{
    return m_daemonLogEpoch;
}

void ConsoleStore::setConsoleOpen(bool open)
{
    if (open && !m_consoleOpen) {
        m_hasUnseenErrors = false;
    }
    m_consoleOpen = open;
}

QString ConsoleStore::nextEntryId()
{
    m_entryCounter += 1;
    return QStringLiteral("log-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(m_entryCounter);
}

void ConsoleStore::append(ConsoleLogSource source,
                          ConsoleLogLevel level,
                          const QString &message,
                          const QString &detail)
{
    ConsoleLogEntry entry;
    entry.id = nextEntryId();
    entry.source = source;
    entry.level = level;
    entry.message = message;
    entry.detail = detail;
    entry.timestampMs = QDateTime::currentMSecsSinceEpoch();
    m_entries.push_back(entry);
    trimEntries();
    applyUnseenErrorOnAppend(entry);
    emitChanged();
}

void ConsoleStore::appendGuiLogLine(const QString &line)
{
    qint64 ts = 0;
    ConsoleLogLevel level = ConsoleLogLevel::Info;
    QString message;
    if (parseGuiLogLine(line, &ts, &level, &message)) {
        ConsoleLogEntry entry;
        entry.id = nextEntryId();
        entry.source = ConsoleLogSource::Gui;
        entry.level = level;
        entry.message = message;
        entry.timestampMs = ts;
        m_entries.push_back(entry);
    } else {
        append(ConsoleLogSource::Gui, ConsoleLogLevel::Info, line);
        return;
    }
    trimEntries();
    applyUnseenErrorOnAppend(m_entries.last());
    emitChanged();
}

void ConsoleStore::appendDaemonLines(const QString &text, int epoch)
{
    if (!m_settings.showDaemonLogs || epoch != m_daemonLogEpoch) {
        return;
    }
    const QStringList lines =
        text.split(QRegularExpression(QStringLiteral(R"(\r?\n)")), Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return;
    }

    QVector<ConsoleLogEntry> additions;
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        ConsoleLogEntry entry;
        entry.id = nextEntryId();
        entry.source = ConsoleLogSource::Daemon;
        entry.level = inferDaemonLogLevel(line);
        entry.message = line;
        entry.timestampMs = QDateTime::currentMSecsSinceEpoch();
        additions.push_back(entry);
    }
    if (additions.isEmpty() || m_daemonLogEpoch != epoch) {
        return;
    }

    for (const ConsoleLogEntry &entry : additions) {
        m_entries.push_back(entry);
    }
    trimEntries();

    const bool hasError =
        std::any_of(additions.begin(), additions.end(), [](const ConsoleLogEntry &e) {
            return e.level == ConsoleLogLevel::Error;
        });
    if (!m_consoleOpen && hasError) {
        m_hasUnseenErrors = true;
    }
    emitChanged();
}

void ConsoleStore::setDaemonLogOffset(qint64 offset)
{
    m_daemonLogOffset = offset;
}

void ConsoleStore::resetDaemonLogCursor()
{
    m_daemonLogOffset = 0;
}

int ConsoleStore::clear()
{
    m_entries.clear();
    m_hasUnseenErrors = false;
    m_daemonLogEpoch += 1;
    emitChanged();
    return m_daemonLogEpoch;
}

void ConsoleStore::markErrorsSeen()
{
    if (!m_hasUnseenErrors) {
        return;
    }
    m_hasUnseenErrors = false;
    emitChanged();
}

void ConsoleStore::setAutoScroll(bool value)
{
    if (m_settings.autoScroll == value) {
        return;
    }
    m_settings.autoScroll = value;
    persistSettings();
    emitChanged();
}

void ConsoleStore::setShowGuiLogs(bool value)
{
    if (m_settings.showGuiLogs == value) {
        return;
    }
    m_settings.showGuiLogs = value;
    if (!value) {
        pruneEntries();
    }
    persistSettings();
    emitChanged();
}

void ConsoleStore::setShowDaemonLogs(bool value)
{
    if (m_settings.showDaemonLogs == value) {
        return;
    }
    m_settings.showDaemonLogs = value;
    if (!value) {
        pruneEntries();
    }
    persistSettings();
    emitChanged();
}

void ConsoleStore::setGuiMinLevel(ConsoleLogLevel level)
{
    if (m_settings.guiMinLevel == level) {
        return;
    }
    const bool tightened = consoleLogLevelRank(level) > consoleLogLevelRank(m_settings.guiMinLevel);
    m_settings.guiMinLevel = level;
    if (tightened) {
        pruneEntries();
    }
    persistSettings();
    emitChanged();
}

void ConsoleStore::setDaemonMinLevel(ConsoleLogLevel level)
{
    if (m_settings.daemonMinLevel == level) {
        return;
    }
    const bool tightened = consoleLogLevelRank(level) > consoleLogLevelRank(m_settings.daemonMinLevel);
    m_settings.daemonMinLevel = level;
    if (tightened) {
        pruneEntries();
    }
    persistSettings();
    emitChanged();
}

bool ConsoleStore::entryVisible(const ConsoleLogEntry &entry) const
{
    if (entry.source == ConsoleLogSource::Gui) {
        return m_settings.showGuiLogs && logLevelMeetsMin(entry.level, m_settings.guiMinLevel);
    }
    return m_settings.showDaemonLogs && logLevelMeetsMin(entry.level, m_settings.daemonMinLevel);
}

QVector<ConsoleLogEntry> ConsoleStore::visibleEntries() const
{
    QVector<ConsoleLogEntry> visible;
    for (const ConsoleLogEntry &entry : m_entries) {
        if (entryVisible(entry)) {
            visible.push_back(entry);
        }
    }
    return visible;
}

void ConsoleStore::trimEntries()
{
    const int max = m_settings.maxEntries;
    if (m_entries.size() <= max) {
        return;
    }
    m_entries = m_entries.mid(m_entries.size() - max);
}

void ConsoleStore::pruneEntries()
{
    QVector<ConsoleLogEntry> kept;
    for (const ConsoleLogEntry &entry : m_entries) {
        if (entryVisible(entry)) {
            kept.push_back(entry);
        }
    }
    m_entries = kept;
}

void ConsoleStore::persistSettings()
{
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    store.beginGroup(QStringLiteral("console"));
    store.setValue(QStringLiteral("maxEntries"), m_settings.maxEntries);
    store.setValue(QStringLiteral("autoScroll"), m_settings.autoScroll);
    store.setValue(QStringLiteral("showGuiLogs"), m_settings.showGuiLogs);
    store.setValue(QStringLiteral("showDaemonLogs"), m_settings.showDaemonLogs);
    store.setValue(QStringLiteral("guiMinLevel"), consoleLogLevelToSettings(m_settings.guiMinLevel));
    store.setValue(QStringLiteral("daemonMinLevel"), consoleLogLevelToSettings(m_settings.daemonMinLevel));
    store.setValue(QStringLiteral("daemonPollIntervalMs"), m_settings.daemonPollIntervalMs);
    store.endGroup();
}

void ConsoleStore::applyUnseenErrorOnAppend(const ConsoleLogEntry &entry)
{
    if (!m_consoleOpen && entryVisible(entry) && entry.level == ConsoleLogLevel::Error) {
        m_hasUnseenErrors = true;
    }
}

void ConsoleStore::emitChanged()
{
    emit changed();
}

} // namespace avar::gui
