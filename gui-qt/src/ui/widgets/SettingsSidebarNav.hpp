#pragma once

#include "settings/SettingsCategory.hpp"

#include <QWidget>

class QButtonGroup;

namespace avar::gui {

class Translator;

class SettingsSidebarNav final : public QWidget {
    Q_OBJECT

public:
    explicit SettingsSidebarNav(Translator &translator, QWidget *parent = nullptr);

    void setCategory(SettingsCategory category);

signals:
    void categoryChanged(SettingsCategory category);

private:
    Translator &m_tr;
    QButtonGroup *m_group = nullptr;
};

} // namespace avar::gui
