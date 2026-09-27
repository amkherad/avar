#pragma once

#include "settings/SettingsCategory.hpp"
#include "ui/settings/SettingsContext.hpp"

#include <QWidget>

class QStackedWidget;

namespace avar::gui {

class SettingsPage final : public QWidget {
    Q_OBJECT

public:
    SettingsPage(const SettingsContext &context, QWidget *parent = nullptr);

    void setCategory(SettingsCategory category);

private:
    SettingsContext m_context;
    QStackedWidget *m_stack = nullptr;
};

} // namespace avar::gui
