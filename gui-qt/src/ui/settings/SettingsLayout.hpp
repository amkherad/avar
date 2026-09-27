#pragma once

class QWidget;

namespace avar::gui {

/** Mirrors gui `.avar-settings-form` — constrains field width inside a scroll area. */
constexpr int kSettingsFormMaxWidth = 420;
constexpr int kSettingsWideFormMaxWidth = 640;

QWidget *wrapSettingsPage(QWidget *content, int maxContentWidth = kSettingsFormMaxWidth);

} // namespace avar::gui
