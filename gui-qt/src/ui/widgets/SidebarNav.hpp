#pragma once

#include <QWidget>

class QButtonGroup;

namespace avar::gui {

class Translator;

class SidebarNav final : public QWidget {
    Q_OBJECT

public:
    explicit SidebarNav(Translator &translator, QWidget *parent = nullptr);

    void setActiveIndex(int index);

signals:
    void itemActivated(int index);

private:
    Translator &m_tr;
    QButtonGroup *m_group = nullptr;
};

} // namespace avar::gui
