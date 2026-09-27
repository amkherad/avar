#include "ui/DesktopShellWindow.hpp"

#include "core/Hosting.hpp"
#include "ui/widgets/WindowTitleBar.hpp"

#include <QMouseEvent>
#include <QVBoxLayout>

namespace avar::gui {

DesktopShellWindow::DesktopShellWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("AvarMainWindow"));
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
#endif
}

void DesktopShellWindow::setShellWidget(QWidget *widget)
{
    auto *frame = new QWidget(this);
    frame->setObjectName(QStringLiteral("AvarDesktopFrame"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);

#if defined(AVAR_GUI_HOSTING_DESKTOP)
    auto *titleBar = new WindowTitleBar(frame);
    titleBar->setTitle(windowTitle());
    connect(titleBar, &WindowTitleBar::closeRequested, this, &QWidget::close);
    connect(titleBar, &WindowTitleBar::minimizeRequested, this, &QWidget::showMinimized);
    layout->addWidget(titleBar);
#endif

    layout->addWidget(widget, 1);
    setCentralWidget(frame);
}

} // namespace avar::gui
