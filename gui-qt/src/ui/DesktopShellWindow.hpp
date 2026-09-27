#pragma once

#include <QMainWindow>

class QEvent;

namespace avar::gui {

class DesktopShellWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit DesktopShellWindow(QWidget *parent = nullptr);

    void setShellWidget(QWidget *widget);

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateShellChrome();

    static constexpr int kWindowedOuterMargin = 10;
};

} // namespace avar::gui
