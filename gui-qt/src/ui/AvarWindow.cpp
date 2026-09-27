#include "ui/AvarWindow.hpp"

#include <QVBoxLayout>

namespace avar::gui {

AvarWindow::AvarWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("AvarWindow"));
}

void AvarWindow::setContentWidget(QWidget *widget)
{
    auto *frame = new QWidget(this);
    frame->setObjectName(QStringLiteral("AvarDesktopFrame"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(widget);
    setCentralWidget(frame);
}

} // namespace avar::gui
