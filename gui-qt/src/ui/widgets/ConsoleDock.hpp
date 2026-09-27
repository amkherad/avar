#pragma once

#include "console/ConsoleLog.hpp"

#include <QTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace avar::gui {

class Translator;
class LayoutPreferences;
class ConsoleStore;
class DaemonClient;
class SyncCoordinator;

class ConsoleDock final : public QWidget {
    Q_OBJECT

public:
    ConsoleDock(Translator &translator,
                LayoutPreferences &layout,
                ConsoleStore &store,
                DaemonClient &daemon,
                SyncCoordinator &sync,
                QWidget *parent = nullptr);

    void setOpen(bool open);
    void retranslateUi();

private:
    void rebuildOutput();
    void scrollToEnd();
    void syncPollTimer();
    void pollDaemonLogs();
    void handleClear();
    void populateLevelCombo(QComboBox *combo, ConsoleLogLevel current);
    ConsoleLogLevel levelFromCombo(QComboBox *combo) const;

    Translator &m_tr;
    LayoutPreferences &m_layout;
    ConsoleStore &m_store;
    DaemonClient &m_daemon;
    SyncCoordinator &m_sync;

    QCheckBox *m_autoScroll = nullptr;
    QCheckBox *m_showGui = nullptr;
    QCheckBox *m_showDaemon = nullptr;
    QComboBox *m_guiLevel = nullptr;
    QComboBox *m_daemonLevel = nullptr;
    QLabel *m_guiSeverityLabel = nullptr;
    QLabel *m_daemonSeverityLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_title = nullptr;
    class AvarButton *m_clearBtn = nullptr;
    class AvarButton *m_closeBtn = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_linesHost = nullptr;
    QVBoxLayout *m_linesLayout = nullptr;
    QTimer *m_pollTimer = nullptr;
};

} // namespace avar::gui
