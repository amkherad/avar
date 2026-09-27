#include "core/Application.hpp"

#include "api/DaemonClient.hpp"
#include "config/AppSettings.hpp"
#include "config/GuiPreferences.hpp"
#include "config/LayoutPreferences.hpp"
#include "session/SessionManager.hpp"
#include "core/CrashHandler.hpp"
#include "core/Hosting.hpp"
#include "core/UnixSignalQuit.hpp"
#include "extension/ExtensionBridgeClient.hpp"
#include "sync/SyncCoordinator.hpp"
#include "i18n/Translator.hpp"
#include "theme/ThemeManager.hpp"

#if defined(AVAR_GUI_QT_EMBED_BACKEND)
#include "backend/EmbeddedDaemon.hpp"
#endif

namespace avar::gui {

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setOrganizationName(QStringLiteral("Avar"));
    setApplicationName(QStringLiteral("gui-qt"));
    setApplicationDisplayName(QStringLiteral("Avar"));

    installCrashHandler();

#if defined(AVAR_GUI_HOSTING_DESKTOP)
    m_signalQuit = std::make_unique<UnixSignalQuit>(*this);
#endif

    m_settings = std::make_unique<AppSettings>();
    m_translator = std::make_unique<Translator>();
    m_translator->setLocale(m_settings->locale());
    connect(m_settings.get(), &AppSettings::localeChanged, this, [this] {
        m_translator->setLocale(m_settings->locale());
    });
    m_guiPreferences = std::make_unique<GuiPreferences>();
    m_layout = std::make_unique<LayoutPreferences>();
    m_sessions = std::make_unique<SessionManager>();
    m_theme = std::make_unique<ThemeManager>(*m_settings);
    m_theme->apply();

    m_daemon = std::make_unique<DaemonClient>(DaemonClient::Options{});

#if defined(AVAR_GUI_QT_EMBED_BACKEND)
    m_embeddedDaemon = std::make_unique<EmbeddedDaemon>();
#endif

    m_sync = std::make_unique<SyncCoordinator>(*m_daemon, *m_settings, *m_guiPreferences);

    auto applySession = [this] {
        m_sync->stop();
        const SessionRecord session = m_sessions->activeSession();
        DaemonClient::Options opts;
        opts.baseUrl = session.baseUrl;
        opts.authToken = session.authToken;
        opts.useRelativeApi = m_settings->useRelativeDaemonApi();
#if defined(AVAR_GUI_QT_EMBED_BACKEND)
        const bool useEmbedded = SessionManager::isBuiltinLocalSession(session);
        if (useEmbedded) {
            m_embeddedDaemon->start();
            m_daemon->setInMemoryTransport(true);
        } else {
            m_embeddedDaemon->stop();
            m_daemon->setInMemoryTransport(false);
        }
#else
        m_daemon->setInMemoryTransport(false);
#endif
        m_daemon->setOptions(opts);
        m_sync->start();
    };
    applySession();
    connect(m_sessions.get(), &SessionManager::activeSessionChanged, this, applySession);

    m_extension = std::make_unique<ExtensionBridgeClient>(*m_settings, *m_guiPreferences);

    if (hostingSupportsExtensionSubprocess(detectHostingMode())) {
        m_extension->ensureBridgeProcess();
    }

}

Application::~Application()
{
#if defined(AVAR_GUI_QT_EMBED_BACKEND)
    if (m_embeddedDaemon) {
        m_embeddedDaemon->stop();
    }
#endif
    if (m_sync) {
        m_sync->stop();
    }
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

GuiPreferences &Application::guiPreferences() const
{
    return *m_guiPreferences;
}

Translator &Application::translator() const
{
    return *m_translator;
}

} // namespace avar::gui
