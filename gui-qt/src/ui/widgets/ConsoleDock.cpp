#include "ui/widgets/ConsoleDock.hpp"

#include "api/DaemonClient.hpp"
#include "config/LayoutPreferences.hpp"
#include "console/ConsoleLog.hpp"
#include "console/ConsoleStore.hpp"
#include "core/GuiLog.hpp"
#include "i18n/Translator.hpp"
#include "sync/SyncCoordinator.hpp"
#include "ui/widgets/AvarButton.hpp"
#include "ui/widgets/ResizeHandle.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace avar::gui {

namespace {

QString formatLogTime(qint64 timestampMs)
{
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestampMs);
    return dt.toString(QStringLiteral("HH:mm:ss"));
}

QString lineLevelClass(ConsoleLogLevel level)
{
    switch (level) {
    case ConsoleLogLevel::Debug:
        return QStringLiteral("AvarConsoleLineDebug");
    case ConsoleLogLevel::Info:
        return QStringLiteral("AvarConsoleLineInfo");
    case ConsoleLogLevel::Warn:
        return QStringLiteral("AvarConsoleLineWarn");
    case ConsoleLogLevel::Error:
        return QStringLiteral("AvarConsoleLineError");
    }
    return QStringLiteral("AvarConsoleLineInfo");
}

QString lineSourceClass(ConsoleLogSource source)
{
    return source == ConsoleLogSource::Gui ? QStringLiteral("AvarConsoleLineGui")
                                           : QStringLiteral("AvarConsoleLineDaemon");
}

} // namespace

ConsoleDock::ConsoleDock(Translator &translator,
                       LayoutPreferences &layout,
                       ConsoleStore &store,
                       DaemonClient &daemon,
                       SyncCoordinator &sync,
                       QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_layout(layout)
    , m_store(store)
    , m_daemon(daemon)
    , m_sync(sync)
{
    setObjectName(QStringLiteral("AvarConsole"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *resize = new ResizeHandle(ResizeAxis::Vertical, this);
    resize->setObjectName(QStringLiteral("AvarConsoleResize"));
    resize->setToolTip(m_tr.tr(QStringLiteral("console.resize")));
    QObject::connect(resize, &ResizeHandle::resizeDelta, &layout, &LayoutPreferences::adjustConsoleHeight);

    auto *header = new QHBoxLayout();
    m_title = new QLabel(m_tr.tr(QStringLiteral("console.title")), this);
    m_title->setProperty("class", QStringLiteral("AvarConsoleTitle"));

    m_autoScroll = new QCheckBox(m_tr.tr(QStringLiteral("console.autoScroll")), this);
    m_autoScroll->setChecked(m_store.settings().autoScroll);
    QObject::connect(m_autoScroll, &QCheckBox::toggled, &m_store, &ConsoleStore::setAutoScroll);

    m_showGui = new QCheckBox(m_tr.tr(QStringLiteral("console.showGui")), this);
    m_showGui->setChecked(m_store.settings().showGuiLogs);
    QObject::connect(m_showGui, &QCheckBox::toggled, &m_store, &ConsoleStore::setShowGuiLogs);

    m_guiSeverityLabel = new QLabel(m_tr.tr(QStringLiteral("console.guiSeverity")), this);
    m_guiSeverityLabel->setProperty("class", QStringLiteral("AvarConsoleSeverityLabel"));
    m_guiLevel = new QComboBox(this);
    populateLevelCombo(m_guiLevel, m_store.settings().guiMinLevel);
    QObject::connect(m_guiLevel, &QComboBox::currentIndexChanged, this, [this] {
        m_store.setGuiMinLevel(levelFromCombo(m_guiLevel));
    });

    m_showDaemon = new QCheckBox(m_tr.tr(QStringLiteral("console.showDaemon")), this);
    m_showDaemon->setChecked(m_store.settings().showDaemonLogs);
    QObject::connect(m_showDaemon, &QCheckBox::toggled, &m_store, &ConsoleStore::setShowDaemonLogs);

    m_daemonSeverityLabel = new QLabel(m_tr.tr(QStringLiteral("console.daemonSeverity")), this);
    m_daemonSeverityLabel->setProperty("class", QStringLiteral("AvarConsoleSeverityLabel"));
    m_daemonLevel = new QComboBox(this);
    populateLevelCombo(m_daemonLevel, m_store.settings().daemonMinLevel);
    QObject::connect(m_daemonLevel, &QComboBox::currentIndexChanged, this, [this] {
        m_store.setDaemonMinLevel(levelFromCombo(m_daemonLevel));
    });

    m_clearBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    m_clearBtn->setText(m_tr.tr(QStringLiteral("console.clear")));
    QObject::connect(m_clearBtn, &QPushButton::clicked, this, &ConsoleDock::handleClear);

    m_closeBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    m_closeBtn->setText(QStringLiteral("×"));
    m_closeBtn->setToolTip(m_tr.tr(QStringLiteral("console.close")));
    QObject::connect(m_closeBtn, &QPushButton::clicked, this, [this, &layout] { layout.setConsoleOpen(false); });

    header->addWidget(m_title);
    header->addSpacing(12);
    header->addWidget(m_autoScroll);
    header->addWidget(m_showGui);
    header->addWidget(m_guiSeverityLabel);
    header->addWidget(m_guiLevel);
    header->addWidget(m_showDaemon);
    header->addWidget(m_daemonSeverityLabel);
    header->addWidget(m_daemonLevel);
    header->addStretch();
    header->addWidget(m_clearBtn);
    header->addWidget(m_closeBtn);

    m_emptyLabel = new QLabel(m_tr.tr(QStringLiteral("console.empty")), this);
    m_emptyLabel->setProperty("class", QStringLiteral("AvarConsoleEmpty"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("AvarConsoleOutput"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_linesHost = new QWidget(m_scroll);
    m_linesLayout = new QVBoxLayout(m_linesHost);
    m_linesLayout->setContentsMargins(8, 8, 8, 8);
    m_linesLayout->setSpacing(2);
    m_linesLayout->addStretch();
    m_scroll->setWidget(m_linesHost);

    outer->addWidget(resize);
    auto *headerWidget = new QWidget(this);
    headerWidget->setProperty("class", QStringLiteral("AvarConsoleHeader"));
    headerWidget->setLayout(header);
    outer->addWidget(headerWidget);
    outer->addWidget(m_emptyLabel);
    outer->addWidget(m_scroll, 1);

    m_pollTimer = new QTimer(this);
    QObject::connect(m_pollTimer, &QTimer::timeout, this, &ConsoleDock::pollDaemonLogs);
    QObject::connect(&m_store, &ConsoleStore::changed, this, &ConsoleDock::rebuildOutput);
    QObject::connect(&m_sync, &SyncCoordinator::connectionStateChanged, this, &ConsoleDock::syncPollTimer);
    QObject::connect(&layout, &LayoutPreferences::layoutChanged, this, [this, &layout] {
        setOpen(layout.consoleOpen());
        syncPollTimer();
    });

    setOpen(layout.consoleOpen());
    rebuildOutput();
    syncPollTimer();
}

void ConsoleDock::retranslateUi()
{
    if (m_title != nullptr) {
        m_title->setText(m_tr.tr(QStringLiteral("console.title")));
    }
    if (m_autoScroll != nullptr) {
        m_autoScroll->setText(m_tr.tr(QStringLiteral("console.autoScroll")));
    }
    if (m_showGui != nullptr) {
        m_showGui->setText(m_tr.tr(QStringLiteral("console.showGui")));
    }
    if (m_showDaemon != nullptr) {
        m_showDaemon->setText(m_tr.tr(QStringLiteral("console.showDaemon")));
    }
    if (m_guiSeverityLabel != nullptr) {
        m_guiSeverityLabel->setText(m_tr.tr(QStringLiteral("console.guiSeverity")));
    }
    if (m_daemonSeverityLabel != nullptr) {
        m_daemonSeverityLabel->setText(m_tr.tr(QStringLiteral("console.daemonSeverity")));
    }
    if (m_emptyLabel != nullptr) {
        m_emptyLabel->setText(m_tr.tr(QStringLiteral("console.empty")));
    }
    if (m_clearBtn != nullptr) {
        m_clearBtn->setText(m_tr.tr(QStringLiteral("console.clear")));
    }
    if (m_closeBtn != nullptr) {
        m_closeBtn->setToolTip(m_tr.tr(QStringLiteral("console.close")));
    }
    if (auto *resize = findChild<ResizeHandle *>(QStringLiteral("AvarConsoleResize"))) {
        resize->setToolTip(m_tr.tr(QStringLiteral("console.resize")));
    }
    if (m_guiLevel != nullptr) {
        populateLevelCombo(m_guiLevel, levelFromCombo(m_guiLevel));
    }
    if (m_daemonLevel != nullptr) {
        populateLevelCombo(m_daemonLevel, levelFromCombo(m_daemonLevel));
    }
}

void ConsoleDock::setOpen(bool open)
{
    m_store.setConsoleOpen(open);
    setVisible(open);
    syncPollTimer();
    if (open) {
        scrollToEnd();
    }
}

void ConsoleDock::populateLevelCombo(QComboBox *combo, ConsoleLogLevel current)
{
    combo->clear();
    const ConsoleLogLevel levels[] = {ConsoleLogLevel::Debug, ConsoleLogLevel::Info, ConsoleLogLevel::Warn,
                                      ConsoleLogLevel::Error};
    int selectIndex = 1;
    for (int i = 0; i < 4; ++i) {
        const QString key =
            QStringLiteral("console.level.%1").arg(consoleLogLevelToSettings(levels[i]));
        combo->addItem(m_tr.tr(key), static_cast<int>(levels[i]));
        if (levels[i] == current) {
            selectIndex = i;
        }
    }
    combo->setCurrentIndex(selectIndex);
}

ConsoleLogLevel ConsoleDock::levelFromCombo(QComboBox *combo) const
{
    return static_cast<ConsoleLogLevel>(combo->currentData().toInt());
}

void ConsoleDock::rebuildOutput()
{
    while (m_linesLayout->count() > 1) {
        QLayoutItem *item = m_linesLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const QVector<ConsoleLogEntry> visible = m_store.visibleEntries();
    m_emptyLabel->setVisible(visible.isEmpty());
    m_scroll->setVisible(!visible.isEmpty());

    for (const ConsoleLogEntry &entry : visible) {
        auto *line = new QLabel(m_linesHost);
        line->setWordWrap(true);
        line->setTextInteractionFlags(Qt::TextSelectableByMouse);
        line->setProperty("class", QStringLiteral("AvarConsoleLine"));
        line->setProperty("lineLevel", lineLevelClass(entry.level));
        line->setProperty("logSource", lineSourceClass(entry.source));
        QString text = QStringLiteral("%1 [%2] %3 %4")
                           .arg(formatLogTime(entry.timestampMs),
                                consoleLogSourceLabel(entry.source),
                                consoleLogLevelLabel(entry.level),
                                entry.message);
        if (!entry.detail.isEmpty()) {
            text += QStringLiteral(" — %1").arg(entry.detail);
        }
        line->setText(text);
        m_linesLayout->insertWidget(m_linesLayout->count() - 1, line);
    }

    if (m_store.settings().autoScroll && m_layout.consoleOpen()) {
        scrollToEnd();
    }

    const ConsoleSettings settings = m_store.settings();
    if (m_pollTimer->interval() != settings.daemonPollIntervalMs) {
        m_pollTimer->setInterval(settings.daemonPollIntervalMs);
    }
}

void ConsoleDock::scrollToEnd()
{
    if (!m_scroll) {
        return;
    }
    QScrollBar *bar = m_scroll->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ConsoleDock::syncPollTimer()
{
    const bool shouldPoll = m_layout.consoleOpen() && m_sync.connectionState() == ConnectionState::Connected
                            && m_store.settings().showDaemonLogs;
    if (shouldPoll) {
        if (!m_pollTimer->isActive()) {
            m_pollTimer->setInterval(m_store.settings().daemonPollIntervalMs);
            m_pollTimer->start();
            pollDaemonLogs();
        }
    } else {
        m_pollTimer->stop();
    }
}

void ConsoleDock::pollDaemonLogs()
{
    if (!m_layout.consoleOpen() || m_sync.connectionState() != ConnectionState::Connected
        || !m_store.settings().showDaemonLogs) {
        return;
    }

    const int epoch = m_store.daemonLogEpoch();
    const qint64 offset = m_store.daemonLogOffset();
    m_daemon.getLogs(80, offset, [this, epoch, offset](bool ok, const QString &logs, qint64 next) {
        if (!ok) {
            GuiLog::instance().warn(m_tr.tr(QStringLiteral("console.daemonFetchFailed")));
            return;
        }
        if (!logs.trimmed().isEmpty()) {
            m_store.appendDaemonLines(logs, epoch);
        }
        if (next > offset) {
            m_store.setDaemonLogOffset(next);
        }
    });
}

void ConsoleDock::handleClear()
{
    const qint64 offset = m_store.daemonLogOffset();
    m_store.clear();
    if (m_sync.connectionState() != ConnectionState::Connected) {
        return;
    }
    m_daemon.skipLogCursor(offset, 80, [this](bool ok, qint64 tail) {
        if (ok) {
            m_store.setDaemonLogOffset(tail);
        }
    });
}

} // namespace avar::gui
