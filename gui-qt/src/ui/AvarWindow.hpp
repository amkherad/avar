#pragma once

#include <QMainWindow>

#include <memory>

class QEvent;
class QLabel;
class QShowEvent;
class QWidget;

namespace avar::gui {

class FramelessShellChrome;
class HeaderWindowDrag;

/** Secondary window with the same chrome and theme as the main shell (desktop). */
class AvarWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit AvarWindow(QWidget *parent = nullptr);
    ~AvarWindow() override;

    void setContentWidget(QWidget *widget);
    void showCentered();

    using QMainWindow::setWindowTitle;

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void ensureChromeBuilt();
    void updateShellChrome();
    void centerOnScreen();

    static constexpr int kWindowedOuterMargin = 10;

    QWidget *m_frame = nullptr;
    QLabel *m_titleLabel = nullptr;
    QWidget *m_contentHost = nullptr;
    HeaderWindowDrag *m_headerDrag = nullptr;
    bool m_chromeBuilt = false;
    bool m_pendingCenter = true;
    std::unique_ptr<FramelessShellChrome> m_chrome;
};

} // namespace avar::gui
