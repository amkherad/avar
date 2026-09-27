#pragma once

#include "settings/SettingsCategory.hpp"
#include "sync/SyncCoordinator.hpp"
#include "theme/ThemeTokens.hpp"

#include <QStackedWidget>
#include <QWidget>

class QPushButton;

namespace avar::gui {

class Translator;
class ThemeManager;
class LayoutPreferences;
class SessionManager;
class ExtensionBridgeClient;
class QueuePanelWidget;
class SessionSelector;
class SettingsSidebarNav;
class HelpSidebarNav;
class HeaderWindowDrag;

enum class AppPage {
    Dashboard = 0,
    Settings = 1,
    Help = 2,
};

class AppShell final : public QWidget {
    Q_OBJECT

public:
    AppShell(Translator &translator,
             ThemeManager &theme,
             LayoutPreferences &layout,
             SessionManager &sessions,
             ExtensionBridgeClient &extensionBridge,
             QWidget *parent = nullptr);

    [[nodiscard]] QStackedWidget *pageStack();
    [[nodiscard]] SettingsSidebarNav *settingsSidebarNav() const;
    [[nodiscard]] QueuePanelWidget *queuePanel() const;
    [[nodiscard]] SessionSelector *sessionSelector() const;

    void setPage(AppPage page);
    void setSettingsCategory(SettingsCategory category);
    void setConnectionState(ConnectionState state);

signals:
    void pageChanged(AppPage page);
    void openSettingsCategory(SettingsCategory category);
    void themeToggleRequested();
    void themeSettingRequested(ThemeSetting setting);

private:
    void buildHeader();

    Translator &m_tr;
    ThemeManager &m_theme;
    LayoutPreferences &m_layout;
    SessionManager &m_sessions;
    ExtensionBridgeClient &m_extension;

    QWidget *m_header = nullptr;
    QPushButton *m_backButton = nullptr;
    QWidget *m_sidebar = nullptr;
    QStackedWidget *m_sidebarBody = nullptr;
    QueuePanelWidget *m_queuePanel = nullptr;
    SessionSelector *m_sessionSelector = nullptr;
    SettingsSidebarNav *m_settingsSidebarNav = nullptr;
    QWidget *m_helpSidebar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QPushButton *m_helpHeaderButton = nullptr;
    QPushButton *m_settingsHeaderButton = nullptr;
    HeaderWindowDrag *m_headerDrag = nullptr;
    AppPage m_page = AppPage::Dashboard;
};

} // namespace avar::gui
