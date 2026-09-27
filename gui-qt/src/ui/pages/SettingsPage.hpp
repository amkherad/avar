#pragma once

#include "settings/SettingsCategory.hpp"

#include <QWidget>

class QStackedWidget;

namespace avar::gui {

class AppSettings;
class DaemonClient;
class Translator;

class SettingsPage final : public QWidget {
    Q_OBJECT

public:
    SettingsPage(Translator &translator, AppSettings &settings, DaemonClient &daemon, QWidget *parent = nullptr);

    void setCategory(SettingsCategory category);

private:
    QWidget *makeGeneralPanel();
    QWidget *makeDownloadsPanel();
    QWidget *makeQueuesPanel();
    QWidget *makeDaemonPanel();
    QWidget *makeBrowserPanel();
    QWidget *makeShortcutsPanel();
    QWidget *makeAboutPanel();

    Translator &m_tr;
    AppSettings &m_settings;
    DaemonClient &m_daemon;
    QStackedWidget *m_stack = nullptr;
};

} // namespace avar::gui
