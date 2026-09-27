#pragma once

#include <QMainWindow>

#include <memory>

class QEvent;

namespace avar::gui {

class FramelessShellChrome;

class DesktopShellWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit DesktopShellWindow(QWidget *parent = nullptr);
    ~DesktopShellWindow() override;

    void setShellWidget(QWidget *widget);
    void setChromeRadius(int radiusPx);

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateShellChrome();

    static constexpr int kWindowedOuterMargin = 10;

    std::unique_ptr<FramelessShellChrome> m_chrome;
};

} // namespace avar::gui
