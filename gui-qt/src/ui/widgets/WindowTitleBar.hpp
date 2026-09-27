#pragma once

#include <QWidget>

class QLabel;

namespace avar::gui {

class WindowTitleBar final : public QWidget {
    Q_OBJECT

public:
    explicit WindowTitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);

signals:
    void closeRequested();
    void minimizeRequested();

private:
    QLabel *m_title = nullptr;
};

} // namespace avar::gui
