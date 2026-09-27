#include "ui/widgets/SessionSelector.hpp"

#include "i18n/Translator.hpp"
#include "session/SessionManager.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <functional>

namespace avar::gui {

namespace {

class SessionTriggerWidget final : public QWidget {
public:
    explicit SessionTriggerWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setProperty("class", QStringLiteral("AvarSessionTrigger"));
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    std::function<void()> onActivated;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mouseReleaseEvent(event);
            return;
        }
        QWidget *child = childAt(event->pos());
        if (qobject_cast<QAbstractButton *>(child) != nullptr) {
            QWidget::mouseReleaseEvent(event);
            return;
        }
        if (onActivated) {
            onActivated();
        }
        event->accept();
    }
};

QString elidedLabelText(const QLabel &label, const QString &text)
{
    int width = label.width();
    if (width <= 0 && label.parentWidget() != nullptr) {
        width = label.parentWidget()->width() - 72;
    }
    if (width <= 0) {
        width = 160;
    }
    const QFontMetrics metrics(label.fontMetrics());
    return metrics.elidedText(text, Qt::ElideRight, width);
}

} // namespace

SessionSelector::SessionSelector(Translator &translator, SessionManager &sessions, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_sessions(sessions)
{
    setObjectName(QStringLiteral("AvarSessionSelector"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 12, 0, 0);
    layout->setSpacing(0);

    auto *trigger = new SessionTriggerWidget(this);
    m_trigger = trigger;
    trigger->onActivated = [this] { toggleMenu(); };

    auto *grid = new QGridLayout(trigger);
    grid->setContentsMargins(9, 9, 10, 9);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(1, 1);

    m_dot = new QLabel(trigger);
    m_dot->setProperty("class", QStringLiteral("AvarConnectionDot"));
    m_dot->setFixedSize(8, 8);

    m_refresh = new QPushButton(QStringLiteral("↻"), trigger);
    m_refresh->setProperty("class", QStringLiteral("AvarSessionRefresh"));
    m_refresh->setFixedSize(22, 22);
    m_refresh->setToolTip(m_tr.tr(QStringLiteral("session.refresh")));
    m_refresh->setCursor(Qt::PointingHandCursor);
    QObject::connect(m_refresh, &QPushButton::clicked, this, [this] {
        if (m_refreshing) {
            return;
        }
        m_refreshing = true;
        m_refresh->setEnabled(false);
        emit refreshRequested();
        m_refreshing = false;
        m_refresh->setEnabled(true);
    });

    m_label = new QLabel(trigger);
    m_label->setProperty("class", QStringLiteral("AvarSessionLabel"));
    m_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    m_status = new QLabel(trigger);
    m_status->setProperty("class", QStringLiteral("AvarSessionStatus"));
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    m_chevron = new QLabel(QStringLiteral("▾"), trigger);
    m_chevron->setProperty("class", QStringLiteral("AvarSessionChevron"));
    m_chevron->setAlignment(Qt::AlignCenter);

    grid->addWidget(m_dot, 0, 0, Qt::AlignHCenter | Qt::AlignVCenter);
    grid->addWidget(m_refresh, 1, 0, Qt::AlignHCenter | Qt::AlignVCenter);
    grid->addWidget(m_label, 0, 1, Qt::AlignVCenter);
    grid->addWidget(m_status, 1, 1, Qt::AlignVCenter);
    grid->addWidget(m_chevron, 0, 2, 2, 1, Qt::AlignVCenter);

    layout->addWidget(trigger);

    m_menuPopup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_menuPopup->setObjectName(QStringLiteral("AvarSessionMenu"));
    m_menuPopup->setProperty("class", QStringLiteral("AvarSessionMenu"));
    m_menuPopup->installEventFilter(this);

    auto *menuOuter = new QVBoxLayout(m_menuPopup);
    menuOuter->setContentsMargins(10, 10, 10, 10);
    menuOuter->setSpacing(8);

    m_menuListHost = new QWidget(m_menuPopup);
    m_menuListLayout = new QVBoxLayout(m_menuListHost);
    m_menuListLayout->setContentsMargins(0, 0, 0, 0);
    m_menuListLayout->setSpacing(6);

    auto *menuScroll = new QScrollArea(m_menuPopup);
    menuScroll->setWidgetResizable(true);
    menuScroll->setFrameShape(QFrame::NoFrame);
    menuScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    menuScroll->setMaximumHeight(320);
    menuScroll->setWidget(m_menuListHost);
    menuOuter->addWidget(menuScroll, 1);

    auto *addBtn = new AvarButton(AvarButtonVariant::Secondary, m_menuPopup);
    addBtn->setProperty("class", QStringLiteral("AvarSessionAdd"));
    addBtn->setText(m_tr.tr(QStringLiteral("session.add")));
    addBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QObject::connect(addBtn, &QPushButton::clicked, this, [this] {
        closeMenu();
    });
    menuOuter->addWidget(addBtn);

    QObject::connect(&m_sessions, &SessionManager::sessionsChanged, this, [this] {
        rebuildMenu();
        updateTrigger();
    });
    QObject::connect(&m_sessions, &SessionManager::activeSessionChanged, this, [this] { updateTrigger(); });

    rebuildMenu();
    updateTrigger();
}

void SessionSelector::setConnectionState(ConnectionState state)
{
    m_connection = state;
    updateTrigger();
}

bool SessionSelector::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_menuPopup && event->type() == QEvent::Hide) {
        setMenuOpen(false);
    }
    return QWidget::eventFilter(watched, event);
}

void SessionSelector::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateTrigger();
}

void SessionSelector::toggleMenu()
{
    if (m_menuPopup->isVisible()) {
        closeMenu();
        return;
    }
    rebuildMenu();
    positionMenu();
    m_menuPopup->show();
    setMenuOpen(true);
}

void SessionSelector::closeMenu()
{
    if (m_menuPopup->isVisible()) {
        m_menuPopup->hide();
    }
    setMenuOpen(false);
}

void SessionSelector::setMenuOpen(bool open)
{
    if (m_menuOpen == open) {
        return;
    }
    m_menuOpen = open;
    m_chevron->setText(open ? QStringLiteral("▴") : QStringLiteral("▾"));
    m_chevron->setProperty("open", open);
    m_chevron->style()->unpolish(m_chevron);
    m_chevron->style()->polish(m_chevron);
}

void SessionSelector::positionMenu()
{
    m_menuPopup->adjustSize();
    const int width = m_trigger->width();
    m_menuPopup->setFixedWidth(width);
    m_menuPopup->adjustSize();

    const QPoint topLeft = m_trigger->mapToGlobal(QPoint(0, 0));
    const int y = topLeft.y() - m_menuPopup->height() - 4;
    m_menuPopup->move(topLeft.x(), y);
}

void SessionSelector::rebuildMenu()
{
    while (QLayoutItem *item = m_menuListLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const QString activeId = m_sessions.activeSessionId();
    for (const SessionRecord &session : m_sessions.sessions()) {
        const bool active = session.id == activeId;
        auto *item = new QPushButton(m_menuListHost);
        item->setProperty("class", QStringLiteral("AvarSessionListItem"));
        item->setProperty("active", active);
        item->setFlat(true);
        item->setCursor(Qt::PointingHandCursor);

        auto *itemLayout = new QVBoxLayout(item);
        itemLayout->setContentsMargins(10, 8, 10, 8);
        itemLayout->setSpacing(2);

        auto *title = new QLabel(session.label, item);
        title->setProperty("class", QStringLiteral("AvarSessionListTitle"));
        title->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        auto *meta = new QLabel(session.baseUrl, item);
        meta->setProperty("class", QStringLiteral("AvarSessionListMeta"));
        meta->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        itemLayout->addWidget(title);
        itemLayout->addWidget(meta);

        QObject::connect(item, &QPushButton::clicked, this, [this, id = session.id] {
            m_sessions.setActiveSessionId(id);
            closeMenu();
        });

        m_menuListLayout->addWidget(item);
    }
}

void SessionSelector::updateTrigger()
{
    const SessionRecord active = m_sessions.activeSession();
    m_label->setText(elidedLabelText(*m_label, active.label));

    const QString statusText = m_connection == ConnectionState::Connected
                                   ? m_tr.tr(QStringLiteral("session.connected"))
                                   : m_connection == ConnectionState::Connecting
                                         ? m_tr.tr(QStringLiteral("session.connecting"))
                                         : m_tr.tr(QStringLiteral("session.disconnected"));
    m_status->setText(elidedLabelText(*m_status, statusText));

    const bool connected = m_connection == ConnectionState::Connected;
    m_dot->setProperty("connected", QVariant(connected));
    m_dot->style()->unpolish(m_dot);
    m_dot->style()->polish(m_dot);
}

} // namespace avar::gui
