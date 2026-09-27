#include "core/Application.hpp"

#include "api/DaemonClient.hpp"
#include "config/AppSettings.hpp"
#include "config/LayoutPreferences.hpp"
#include "session/SessionManager.hpp"
#include "core/CrashHandler.hpp"
#include "core/Hosting.hpp"
#include "extension/ExtensionBridgeClient.hpp"
#include "sync/SyncCoordinator.hpp"
#include "theme/ThemeManager.hpp"

namespace avar::gui {

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setOrganizationName(QStringLiteral("Avar"));
    setApplicationName(QStringLiteral("gui-qt"));
    setApplicationDisplayName(QStringLiteral("Avar"));

    installCrashHandler();

    m_settings = std::make_unique<AppSettings>();
    m_layout = std::make_unique<LayoutPreferences>();
    m_sessions = std::make_unique<SessionManager>();
    m_theme = std::make_unique<ThemeManager>(*m_settings);
    m_theme->apply();

    m_daemon = std::make_unique<DaemonClient>(DaemonClient::Options{});

    auto applySession = [this] {
        const SessionRecord session = m_sessions->activeSession();
        DaemonClient::Options opts;
        opts.baseUrl = session.baseUrl;
        opts.authToken = session.authToken;
        opts.useRelativeApi = m_settings->useRelativeDaemonApi();
        m_daemon->setOptions(opts);
    };
    applySession();
    connect(m_sessions.get(), &SessionManager::activeSessionChanged, this, applySession);

    m_sync = std::make_unique<SyncCoordinator>(*m_daemon, *m_settings);
    m_extension = std::make_unique<ExtensionBridgeClient>(*m_settings);

    if (hostingSupportsExtensionSubprocess(detectHostingMode())) {
        m_extension->ensureBridgeProcess();
    }

    m_sync->start();
}

Application::~Application()
{
}

ThemeManager &Application::themeManager() const
{
    return *m_theme;
}

AppSettings &Application::settings() const
{
    return *m_settings;
}

DaemonClient &Application::daemonClient() const
{
    return *m_daemon;
}

SyncCoordinator &Application::syncCoordinator() const
{
    return *m_sync;
}

ExtensionBridgeClient &Application::extensionBridge() const
{
    return *m_extension;
}

SessionManager &Application::sessions() const
{
    return *m_sessions;
}

LayoutPreferences &Application::layout() const
{
    return *m_layout;
}

} // namespace avar::gui
