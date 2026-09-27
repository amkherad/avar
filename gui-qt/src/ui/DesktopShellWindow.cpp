#include "ui/DesktopShellWindow.hpp"

#include <QEvent>
#include <QLayout>
#include <QStyle>
#include <QVBoxLayout>

namespace avar::gui {

DesktopShellWindow::DesktopShellWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("AvarMainWindow"));
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(1024, 640);
}

void DesktopShellWindow::setShellWidget(QWidget *widget)
{
    auto *outer = new QWidget(this);
    outer->setObjectName(QStringLiteral("AvarDesktopOuter"));
    auto *outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(10, 10, 10, 10);
    outerLayout->setSpacing(0);

    auto *frame = new QWidget(outer);
    frame->setObjectName(QStringLiteral("AvarDesktopFrame"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(widget, 1);

    outerLayout->addWidget(frame, 1);
    setCentralWidget(outer);
    updateShellChrome();
}

void DesktopShellWindow::updateShellChrome()
{
    QWidget *outer = centralWidget();
    if (outer == nullptr) {
        return;
    }

    const bool maximized = isMaximized();
    const int margin = maximized ? 0 : kWindowedOuterMargin;

    if (QLayout *outerLayout = outer->layout()) {
        outerLayout->setContentsMargins(margin, margin, margin, margin);
    }

    const auto polishProperty = [](QWidget *widget, const char *name, bool value) {
        if (widget == nullptr) {
            return;
        }
        widget->setProperty(name, value);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    };

    polishProperty(outer, "maximized", maximized);

    QWidget *frame = outer->findChild<QWidget *>(QStringLiteral("AvarDesktopFrame"));
    polishProperty(frame, "maximized", maximized);

    QWidget *header = findChild<QWidget *>(QStringLiteral("AvarHeader"));
    polishProperty(header, "maximized", maximized);
}

void DesktopShellWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateShellChrome();
    }
}

} // namespace avar::gui
