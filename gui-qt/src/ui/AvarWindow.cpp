#include "ui/AvarWindow.hpp"

#include <QVBoxLayout>

namespace avar::gui {

AvarWindow::AvarWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("AvarWindow"));
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
#endif
}

void AvarWindow::setContentWidget(QWidget *widget)
{
    auto *frame = new QWidget(this);
    frame->setObjectName(QStringLiteral("AvarDesktopFrame"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(widget);
    setCentralWidget(frame);
}

} // namespace avar::gui
