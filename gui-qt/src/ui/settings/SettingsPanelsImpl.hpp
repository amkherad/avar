#pragma once

class QWidget;

namespace avar::gui {

struct SettingsContext;

QWidget *buildDownloadSettingsPanel(const SettingsContext &ctx, QWidget *parent);
QWidget *buildQueuesSettingsPanel(const SettingsContext &ctx, QWidget *parent);
QWidget *buildDaemonSettingsPanel(const SettingsContext &ctx, QWidget *parent);
QWidget *buildBrowserSettingsPanel(const SettingsContext &ctx, QWidget *parent);
QWidget *buildShortcutsSettingsPanel(const SettingsContext &ctx, QWidget *parent);
QWidget *buildAboutSettingsPanel(const SettingsContext &ctx, QWidget *parent);

} // namespace avar::gui
