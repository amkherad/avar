#pragma once

#include "console/ConsoleLog.hpp"

#include <QObject>
#include <QVector>

namespace avar::gui {

struct ConsoleSettings {
    int maxEntries = 500;
    bool autoScroll = true;
    bool showGuiLogs = true;
    bool showDaemonLogs = true;
    ConsoleLogLevel guiMinLevel = ConsoleLogLevel::Info;
    ConsoleLogLevel daemonMinLevel = ConsoleLogLevel::Info;
    int daemonPollIntervalMs = 3000;
};

struct ConsoleLogEntry {
    QString id;
    ConsoleLogSource source = ConsoleLogSource::Gui;
    ConsoleLogLevel level = ConsoleLogLevel::Info;
    QString message;
    QString detail;
    qint64 timestampMs = 0;
};

class ConsoleStore final : public QObject {
    Q_OBJECT

public:
    explicit ConsoleStore(QObject *parent = nullptr);

    [[nodiscard]] const QVector<ConsoleLogEntry> &entries() const;
    [[nodiscard]] ConsoleSettings settings() const;
    [[nodiscard]] bool hasUnseenErrors() const;
    [[nodiscard]] qint64 daemonLogOffset() const;
    [[nodiscard]] int daemonLogEpoch() const;

    void setConsoleOpen(bool open);

    void append(ConsoleLogSource source,
                ConsoleLogLevel level,
                const QString &message,
                const QString &detail = QString());
    void appendGuiLogLine(const QString &line);
    void appendDaemonLines(const QString &text, int epoch);
    void setDaemonLogOffset(qint64 offset);
    void resetDaemonLogCursor();
    int clear();
    void markErrorsSeen();

    void setAutoScroll(bool value);
    void setShowGuiLogs(bool value);
    void setShowDaemonLogs(bool value);
    void setGuiMinLevel(ConsoleLogLevel level);
    void setDaemonMinLevel(ConsoleLogLevel level);

    [[nodiscard]] bool entryVisible(const ConsoleLogEntry &entry) const;
    [[nodiscard]] QVector<ConsoleLogEntry> visibleEntries() const;

signals:
    void changed();

private:
    QString nextEntryId();
    void trimEntries();
    void pruneEntries();
    void persistSettings();
    void applyUnseenErrorOnAppend(const ConsoleLogEntry &entry);
    void emitChanged();

    QVector<ConsoleLogEntry> m_entries;
    ConsoleSettings m_settings;
    bool m_hasUnseenErrors = false;
    bool m_consoleOpen = false;
    qint64 m_daemonLogOffset = 0;
    int m_daemonLogEpoch = 0;
    qint64 m_entryCounter = 0;
};

} // namespace avar::gui
