#include "ui/DesktopShellWindow.hpp"

#include "ui/FramelessShellChrome.hpp"

#include <QEvent>
#include <QLayout>
#include <QStyle>
#include <QVBoxLayout>

namespace avar::gui {

namespace {

void enableDesktopChrome(QWidget *widget)
{
    if (widget != nullptr) {
        widget->setAttribute(Qt::WA_StyledBackground, true);
    }
}

} // namespace

DesktopShellWindow::~DesktopShellWindow() = default;

DesktopShellWindow::DesktopShellWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("AvarMainWindow"));
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(1024, 640);
    m_chrome = std::make_unique<FramelessShellChrome>(*this);
}

void DesktopShellWindow::setShellWidget(QWidget *widget)
{
    auto *outer = new QWidget(this);
    outer->setObjectName(QStringLiteral("AvarDesktopOuter"));
    enableDesktopChrome(outer);
    auto *outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(kWindowedOuterMargin, kWindowedOuterMargin, kWindowedOuterMargin,
                                    kWindowedOuterMargin);
    outerLayout->setSpacing(0);

    auto *frame = new QWidget(outer);
    frame->setObjectName(QStringLiteral("AvarDesktopFrame"));
    enableDesktopChrome(frame);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(widget, 1);

    outerLayout->addWidget(frame, 1);
    setCentralWidget(outer);
    if (m_chrome != nullptr) {
        m_chrome->setFrame(frame);
        m_chrome->setLayoutChangeCallback([this] { updateShellChrome(); });
    }
    updateShellChrome();
}

void DesktopShellWindow::setChromeRadius(int radiusPx)
{
    if (m_chrome != nullptr) {
        m_chrome->setChromeRadius(radiusPx);
        m_chrome->sync(windowIsMaximized(*this));
    }
}

void DesktopShellWindow::updateShellChrome()
{
    QWidget *outer = centralWidget();
    if (outer == nullptr) {
        return;
    }

    const bool maximized = windowIsMaximized(*this);
    const int margin = maximized ? 0 : kWindowedOuterMargin;

    applyFramelessPresentation(*this, maximized);

    if (QLayout *outerLayout = outer->layout()) {
        outerLayout->setContentsMargins(margin, margin, margin, margin);
    }

    polishDynamicFlag(outer, "maximized", maximized);

    QWidget *frame = outer->findChild<QWidget *>(QStringLiteral("AvarDesktopFrame"));
    polishDynamicFlag(frame, "maximized", maximized);

    QWidget *header = findChild<QWidget *>(QStringLiteral("AvarHeader"));
    polishDynamicFlag(header, "maximized", maximized);

    QWidget *root = findChild<QWidget *>(QStringLiteral("AvarRoot"));
    polishDynamicFlag(root, "maximized", maximized);

    if (m_chrome != nullptr) {
        m_chrome->sync(maximized);
    }
}

void DesktopShellWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateShellChrome();
    }
}

} // namespace avar::gui
