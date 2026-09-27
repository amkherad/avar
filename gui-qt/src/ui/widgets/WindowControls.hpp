#pragma once

#include <QWidget>

class QPushButton;

namespace avar::gui {

class WindowControls final : public QWidget {
    Q_OBJECT

public:
    explicit WindowControls(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refreshMaximizedState();
    void toggleMaximize();
    QPushButton *m_maximizeBtn = nullptr;
    bool m_maximized = false;
};

} // namespace avar::gui
