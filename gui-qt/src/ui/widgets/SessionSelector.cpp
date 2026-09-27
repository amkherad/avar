#include "ui/widgets/SessionSelector.hpp"

#include "i18n/Translator.hpp"
#include "session/SessionManager.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace avar::gui {

SessionSelector::SessionSelector(Translator &translator, SessionManager &sessions, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_sessions(sessions)
{
    setObjectName(QStringLiteral("AvarSessionSelector"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *row = new QHBoxLayout();
    auto *tool = new QToolButton(this);
    m_trigger = tool;
    tool->setObjectName(QStringLiteral("AvarSessionTrigger"));
    tool->setProperty("class", QStringLiteral("AvarSessionTrigger"));
    tool->setPopupMode(QToolButton::InstantPopup);
    m_menu = new QMenu(this);
    connect(tool, &QToolButton::clicked, this, [this, tool] {
        const QPoint above = tool->mapToGlobal(QPoint(0, -m_menu->sizeHint().height() - 4));
        m_menu->exec(above);
    });

    m_refresh = new QPushButton(QStringLiteral("↻"), this);
    m_refresh->setObjectName(QStringLiteral("AvarSessionRefresh"));
    m_refresh->setProperty("class", QStringLiteral("AvarSessionRefresh"));
    m_refresh->setFixedSize(28, 28);
    connect(m_refresh, &QPushButton::clicked, this, &SessionSelector::refreshRequested);

    row->addWidget(m_trigger, 1);
    row->addWidget(m_refresh);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("AvarSessionStatus"));
    m_status->setProperty("class", QStringLiteral("AvarSessionStatus"));

    layout->addLayout(row);
    layout->addWidget(m_status);

    connect(&m_sessions, &SessionManager::sessionsChanged, this, [this] { rebuildMenu(); });
    connect(&m_sessions, &SessionManager::activeSessionChanged, this, [this] { updateLabels(); });

    rebuildMenu();
    updateLabels();
}

void SessionSelector::setConnectionState(ConnectionState state)
{
    m_connection = state;
    updateLabels();
}

void SessionSelector::rebuildMenu()
{
    m_menu->clear();
    for (const SessionRecord &session : m_sessions.sessions()) {
        QAction *action = m_menu->addAction(session.label);
        connect(action, &QAction::triggered, this, [this, id = session.id] {
            m_sessions.setActiveSessionId(id);
        });
    }
    m_menu->addSeparator();
    QAction *add = m_menu->addAction(m_tr.tr(QStringLiteral("session.add")));
    connect(add, &QAction::triggered, this, [] {});
    updateLabels();
}

void SessionSelector::updateLabels()
{
    const SessionRecord active = m_sessions.activeSession();
    const QString dot = m_connection == ConnectionState::Connected ? QStringLiteral("🟢") : QStringLiteral("🔴");
    if (auto *tool = qobject_cast<QToolButton *>(m_trigger)) {
        tool->setText(QStringLiteral("%1 %2").arg(dot, active.label));
    }

    const QString statusText = m_connection == ConnectionState::Connected
                                   ? m_tr.tr(QStringLiteral("session.connected"))
                                   : m_connection == ConnectionState::Connecting
                                         ? m_tr.tr(QStringLiteral("session.connecting"))
                                         : m_tr.tr(QStringLiteral("session.disconnected"));
    m_status->setText(statusText);
}

} // namespace avar::gui
