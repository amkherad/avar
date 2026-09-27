#pragma once

#include "settings/SettingsCategory.hpp"

class QStackedWidget;
class QWidget;

namespace avar::gui {

struct SettingsContext;

void populateSettingsStack(QStackedWidget *stack, const SettingsContext &ctx, QWidget *parent);

void showSettingsCategory(QStackedWidget *stack, SettingsCategory category);

} // namespace avar::gui
