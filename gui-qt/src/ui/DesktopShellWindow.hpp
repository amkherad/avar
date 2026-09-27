#pragma once

#include <QMainWindow>

namespace avar::gui {

class DesktopShellWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit DesktopShellWindow(QWidget *parent = nullptr);

    void setShellWidget(QWidget *widget);
};

} // namespace avar::gui
