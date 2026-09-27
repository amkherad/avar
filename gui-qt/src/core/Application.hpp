#pragma once

#include <QApplication>
#include <memory>

namespace avar::gui {

class ThemeManager;
class AppSettings;
class DaemonClient;
class SyncCoordinator;
class ExtensionBridgeClient;
class SessionManager;
class LayoutPreferences;
class GuiPreferences;
class EmbeddedDaemon;

class Application final : public QApplication {
    Q_OBJECT

public:
    Application(int &argc, char **argv);
    ~Application() override;

    [[nodiscard]] ThemeManager &themeManager() const;
    [[nodiscard]] AppSettings &settings() const;
    [[nodiscard]] DaemonClient &daemonClient() const;
    [[nodiscard]] SyncCoordinator &syncCoordinator() const;
    [[nodiscard]] ExtensionBridgeClient &extensionBridge() const;
    [[nodiscard]] SessionManager &sessions() const;
    [[nodiscard]] LayoutPreferences &layout() const;
    [[nodiscard]] GuiPreferences &guiPreferences() const;

private:
    std::unique_ptr<ThemeManager> m_theme;
    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<GuiPreferences> m_guiPreferences;
    std::unique_ptr<DaemonClient> m_daemon;
    std::unique_ptr<SyncCoordinator> m_sync;
    std::unique_ptr<ExtensionBridgeClient> m_extension;
    std::unique_ptr<SessionManager> m_sessions;
    std::unique_ptr<LayoutPreferences> m_layout;
#if defined(AVAR_GUI_QT_EMBED_BACKEND)
    std::unique_ptr<EmbeddedDaemon> m_embeddedDaemon;
#endif
};

} // namespace avar::gui
