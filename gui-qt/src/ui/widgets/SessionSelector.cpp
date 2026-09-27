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
#include <QPainter>
#include <QTransform>

#include <functional>

namespace avar::gui {

namespace {

class SessionListItemFrame final : public QFrame {
public:
    explicit SessionListItemFrame(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setProperty("class", QStringLiteral("AvarSessionListItem"));
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    }

    std::function<void()> onActivated;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && onActivated) {
            onActivated();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }
};

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

QPixmap dropdownChevronPixmap(const QColor &color, int size, bool open)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (open) {
        QTransform transform;
        transform.translate(size / 2.0, size / 2.0);
        transform.rotate(180);
        transform.translate(-size / 2.0, -size / 2.0);
        painter.setTransform(transform);
    }
    QPen pen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const qreal inset = size * 0.28;
    const qreal midY = size * 0.62;
    const qreal topY = size * 0.38;
    const QPointF points[] = {
        QPointF(inset, topY),
        QPointF(size / 2.0, midY),
        QPointF(size - inset, topY),
    };
    painter.drawPolyline(points, 3);
    return pixmap;
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

    m_chevron = new QLabel(trigger);
    m_chevron->setProperty("class", QStringLiteral("AvarSessionChevron"));
    m_chevron->setAlignment(Qt::AlignCenter);
    m_chevron->setFixedSize(16, 16);

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

    m_menuScroll = new QScrollArea(m_menuPopup);
    m_menuScroll->setObjectName(QStringLiteral("AvarSessionMenuScroll"));
    m_menuScroll->setWidgetResizable(true);
    m_menuScroll->setFrameShape(QFrame::NoFrame);
    m_menuScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_menuScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_menuScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_menuScroll->setMaximumHeight(320);
    m_menuScroll->setWidget(m_menuListHost);
    menuOuter->addWidget(m_menuScroll);

    auto *addBtn = new AvarButton(AvarButtonVariant::Secondary, m_menuPopup);
    addBtn->setProperty("class", QStringLiteral("AvarSessionAdd"));
    addBtn->setText(m_tr.tr(QStringLiteral("session.add")));
    addBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QObject::connect(addBtn, &QPushButton::clicked, this, [this] {
        closeMenu();
        emit addSessionRequested();
    });
    menuOuter->addWidget(addBtn);

    QObject::connect(&m_sessions, &SessionManager::sessionsChanged, this, [this] {
        rebuildMenu();
        updateTrigger();
    });
    QObject::connect(&m_sessions, &SessionManager::activeSessionChanged, this, [this] { updateTrigger(); });

    rebuildMenu();
    updateTrigger();
    updateChevronIcon();
}

void SessionSelector::setConnectionState(ConnectionState state)
{
    m_connection = state;
    updateTrigger();
}

void SessionSelector::retranslateUi()
{
    if (m_refresh != nullptr) {
        m_refresh->setToolTip(m_tr.tr(QStringLiteral("session.refresh")));
    }
    if (m_menuPopup != nullptr) {
        rebuildMenu();
    }
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

void SessionSelector::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::ThemeChange) {
        updateChevronIcon();
    }
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
    m_chevron->setProperty("open", open);
    updateChevronIcon();
}

void SessionSelector::updateChevronIcon()
{
    if (m_chevron == nullptr) {
        return;
    }
    const QColor color = palette().color(QPalette::PlaceholderText);
    m_chevron->setPixmap(dropdownChevronPixmap(color, 16, m_menuOpen));
}

void SessionSelector::updateMenuScrollHeight()
{
    if (m_menuScroll == nullptr || m_menuListHost == nullptr) {
        return;
    }

    const int width = m_trigger != nullptr ? m_trigger->width() - 20 : m_menuPopup->width();
    if (width > 0) {
        m_menuListHost->setMinimumWidth(qMax(0, width - 8));
    }

    m_menuListHost->adjustSize();
    const int contentHeight = m_menuListHost->sizeHint().height();
    const int maxHeight = 320;
    const int scrollHeight = contentHeight > 0 ? qMin(contentHeight, maxHeight) : 0;
    m_menuScroll->setMinimumHeight(scrollHeight);
    m_menuScroll->setMaximumHeight(maxHeight);
    if (scrollHeight > 0) {
        m_menuScroll->setFixedHeight(scrollHeight);
    }
}

void SessionSelector::positionMenu()
{
    const int width = m_trigger->width();
    m_menuPopup->setFixedWidth(width);
    updateMenuScrollHeight();
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
        auto *item = new SessionListItemFrame(m_menuListHost);
        item->setProperty("active", active);
        item->style()->unpolish(item);
        item->style()->polish(item);

        auto *itemLayout = new QVBoxLayout(item);
        itemLayout->setContentsMargins(10, 8, 10, 8);
        itemLayout->setSpacing(4);

        auto *title = new QLabel(session.label, item);
        title->setProperty("class", QStringLiteral("AvarSessionListTitle"));
        title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

        auto *meta = new QLabel(item);
        meta->setProperty("class", QStringLiteral("AvarSessionListMeta"));
        meta->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        meta->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        meta->setText(elidedLabelText(*meta, session.baseUrl));

        itemLayout->addWidget(title);
        itemLayout->addWidget(meta);

        item->onActivated = [this, id = session.id] {
            m_sessions.setActiveSessionId(id);
            closeMenu();
        };

        m_menuListLayout->addWidget(item);
    }

    updateMenuScrollHeight();
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
