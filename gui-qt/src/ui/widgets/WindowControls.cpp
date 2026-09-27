#include "ui/widgets/WindowControls.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include <QToolTip>

namespace avar::gui {

namespace {

class WindowControlButton final : public QPushButton {
public:
    explicit WindowControlButton(const QString &glyphClass, const QString &label, QWidget *parent = nullptr)
        : QPushButton(label, parent)
    {
        setProperty("class", QStringLiteral("AvarWindowControlBtn"));
        setProperty("glyph", glyphClass);
        setFlat(true);
        setFixedSize(46, 44);
        setFocusPolicy(Qt::NoFocus);
    }

    void setGlyph(const QString &glyphClass, const QString &label)
    {
        setProperty("glyph", glyphClass);
        setText(label);
        style()->unpolish(this);
        style()->polish(this);
    }
};

} // namespace

WindowControls::WindowControls(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("AvarWindowControls"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *minBtn = new WindowControlButton(QStringLiteral("minimize"), QStringLiteral("−"), this);
    minBtn->setToolTip(QStringLiteral("Minimize"));
    m_maximizeBtn = new WindowControlButton(QStringLiteral("maximize"), QStringLiteral("□"), this);
    m_maximizeBtn->setToolTip(QStringLiteral("Maximize"));
    auto *closeBtn = new WindowControlButton(QStringLiteral("close"), QStringLiteral("×"), this);
    closeBtn->setProperty("class", QStringLiteral("AvarWindowControlBtn AvarWindowControlBtnClose"));
    closeBtn->setToolTip(QStringLiteral("Close"));

    QObject::connect(minBtn, &QPushButton::clicked, this, [this] {
        if (QWidget *top = window()) {
            top->showMinimized();
        }
    });
    QObject::connect(m_maximizeBtn, &QPushButton::clicked, this, &WindowControls::toggleMaximize);
    QObject::connect(closeBtn, &QPushButton::clicked, this, [this] {
        if (QWidget *top = window()) {
            top->close();
        }
    });

    layout->addWidget(minBtn);
    layout->addWidget(m_maximizeBtn);
    layout->addWidget(closeBtn);
}

void WindowControls::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        refreshMaximizedState();
    }
}

void WindowControls::refreshMaximizedState()
{
    QWidget *top = window();
    if (!top || !m_maximizeBtn) {
        return;
    }
    m_maximized = top->isMaximized();
    m_maximizeBtn->setProperty("glyph", m_maximized ? QStringLiteral("restore") : QStringLiteral("maximize"));
    m_maximizeBtn->setText(m_maximized ? QStringLiteral("❐") : QStringLiteral("□"));
    m_maximizeBtn->setToolTip(m_maximized ? QStringLiteral("Restore") : QStringLiteral("Maximize"));
    m_maximizeBtn->style()->unpolish(m_maximizeBtn);
    m_maximizeBtn->style()->polish(m_maximizeBtn);
}

void WindowControls::toggleMaximize()
{
    QWidget *top = window();
    if (!top) {
        return;
    }
    if (top->isMaximized()) {
        top->showNormal();
    } else {
        top->showMaximized();
    }
    refreshMaximizedState();
}

} // namespace avar::gui
