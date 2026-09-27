#include "ui/MainWindow.hpp"

#include "api/DaemonTypes.hpp"
#include "config/AppSettings.hpp"
#include "core/Application.hpp"
#include "core/GuiLog.hpp"
#include "i18n/AppLocale.hpp"
#include "i18n/Translator.hpp"
#include "models/DownloadTableModel.hpp"
#include "models/QueueListModel.hpp"
#include "settings/SettingsCategory.hpp"
#include "sync/SyncCoordinator.hpp"
#include "theme/ThemeManager.hpp"
#include "ui/AvarWindow.hpp"
#include "ui/DesktopShellWindow.hpp"
#include "ui/DesktopTray.hpp"
#include "ui/pages/DashboardPage.hpp"
#include "ui/pages/AddDownloadPopupPage.hpp"
#include "ui/pages/BatchAddDownloadsPopupPage.hpp"
#include "ui/pages/SessionEditorPopupPage.hpp"
#include "ui/pages/HelpPage.hpp"
#include "ui/pages/SettingsPage.hpp"
#include "ui/settings/SettingsContext.hpp"
#include "ui/widgets/QueuePanelWidget.hpp"
#include "ui/widgets/SessionSelector.hpp"
#include "ui/widgets/HelpSidebarNav.hpp"
#include "ui/widgets/SettingsSidebarNav.hpp"

#include <QJsonObject>
#include <QInputDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QTimer>
#include <memory>

namespace avar::gui {

MainWindow::~MainWindow() = default;

MainWindow::MainWindow(Application &app, QWidget *parent)
    : QWidget(parent)
    , m_app(app)
{
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    m_shellWindow = std::make_unique<DesktopShellWindow>();
#endif

    QObject::connect(&app.translator(), &Translator::translationsChanged, this, &MainWindow::applyLocalizedUi);
    m_downloads = std::make_unique<DownloadTableModel>(app.translator());
    m_queues = std::make_unique<QueueListModel>();

    m_shell = new AppShell(app.translator(), app.themeManager(), app.layout(), app.sessions(), app.extensionBridge(),
                           app.settings(), app.guiPreferences(), this);

    QStackedWidget *stack = m_shell->pageStack();
    m_dashboard = new DashboardPage(app.translator(), app.layout(), app.daemonClient(), app.syncCoordinator(),
                                    *m_downloads, stack);
    const SettingsContext settingsCtx{app.translator(), app.settings(),     app.guiPreferences(), app.daemonClient(),
                                      app.sessions(),   app.extensionBridge()};
    m_settings = new SettingsPage(settingsCtx, stack);
    m_help = new HelpPage(app.translator(), app.settings(), stack);
    stack->addWidget(m_dashboard);
    stack->addWidget(m_settings);
    stack->addWidget(m_help);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_shell);

#if defined(AVAR_GUI_HOSTING_DESKTOP)
    m_shellWindow->setShellWidget(this);
    m_shellWindow->resize(1150, 760);
    m_shellWindow->setWindowTitle(QStringLiteral("Avar"));
#else
    setWindowTitle(QStringLiteral("Avar"));
    resize(1150, 760);
#endif

    connect(m_shell, &AppShell::pageChanged, stack, [stack](AppPage page) {
        stack->setCurrentIndex(static_cast<int>(page));
    });

    connect(m_shell, &AppShell::openSettingsCategory, this, [this](SettingsCategory category) {
        m_shell->setPage(AppPage::Settings);
        m_settings->setCategory(category);
        m_shell->setSettingsCategory(category);
    });

    if (SettingsSidebarNav *nav = m_shell->settingsSidebarNav()) {
        connect(nav, &SettingsSidebarNav::categoryChanged, m_settings, &SettingsPage::setCategory);
    }

    if (HelpSidebarNav *helpNav = m_shell->helpSidebarNav()) {
        connect(helpNav, &HelpSidebarNav::topicChanged, m_help, &HelpPage::setTopicId);
    }

    connect(m_shell, &AppShell::themeToggleRequested, &app.themeManager(), &ThemeManager::toggleResolvedTheme);
    connect(m_shell, &AppShell::themeSettingRequested, &app.themeManager(), &ThemeManager::setSetting);

    connect(m_shell->queuePanel(), &QueuePanelWidget::queueSelected, m_dashboard,
            &DashboardPage::setQueueFilterId);

    connect(m_shell->queuePanel(), &QueuePanelWidget::openQueueSettingsRequested, this, [this] {
        m_shell->setPage(AppPage::Settings);
        m_settings->setCategory(SettingsCategory::Queues);
        m_shell->setSettingsCategory(SettingsCategory::Queues);
    });

    connect(m_shell->queuePanel(), &QueuePanelWidget::addQueueRequested, this, [this] {
        const QString name =
            QInputDialog::getText(this, m_app.translator().tr(QStringLiteral("queue.add")),
                                  m_app.translator().tr(QStringLiteral("queue.nameLabel")));
        if (name.isEmpty()) {
            return;
        }
        QJsonObject params;
        params.insert(QStringLiteral("name"), name);
        m_app.daemonClient().addQueue(params, [](bool, const QString &, const QString &) {});
    });

    connect(m_dashboard, &DashboardPage::addDownloadRequested, this, [this] {
        auto *dialog = new AvarWindow(this);
        dialog->setWindowTitle(m_app.translator().tr(QStringLiteral("download.add")));
        auto *page = new AddDownloadPopupPage(m_app.translator(), m_app.daemonClient(), dialog);
        page->setDefaultQueue(m_shell->queuePanel()->selectedQueueId());
        dialog->setContentWidget(page);
        dialog->resize(480, 200);
        connect(page, &AddDownloadPopupPage::accepted, dialog, [dialog, this](const QString &url) {
            GuiLog::instance().info(QStringLiteral("Queued download %1").arg(url));
            dialog->close();
            dialog->deleteLater();
        });
        connect(page, &AddDownloadPopupPage::cancelled, dialog, [dialog] {
            dialog->close();
            dialog->deleteLater();
        });
        dialog->showCentered();
    });

    connect(m_dashboard, &DashboardPage::batchAddRequested, this, [this] {
        auto *dialog = new AvarWindow(this);
        dialog->setWindowTitle(m_app.translator().tr(QStringLiteral("download.batchAdd.button")));
        auto *page = new BatchAddDownloadsPopupPage(m_app.translator(), m_app.daemonClient(), dialog);
        page->setDefaultQueue(m_shell->queuePanel()->selectedQueueId());
        dialog->setContentWidget(page);
        dialog->resize(520, 360);
        connect(page, &BatchAddDownloadsPopupPage::accepted, dialog, [dialog](int count) {
            GuiLog::instance().info(QStringLiteral("Batch queued %1 downloads").arg(count));
            dialog->close();
            dialog->deleteLater();
        });
        connect(page, &BatchAddDownloadsPopupPage::cancelled, dialog, [dialog] {
            dialog->close();
            dialog->deleteLater();
        });
        dialog->showCentered();
    });

    connect(m_shell->sessionSelector(), &SessionSelector::refreshRequested, &app.syncCoordinator(),
            [this] { m_app.syncCoordinator().start(); });

    connect(m_shell->sessionSelector(), &SessionSelector::addSessionRequested, this, [this] {
        auto *dialog = new AvarWindow(this);
        dialog->setWindowTitle(m_app.translator().tr(QStringLiteral("session.add")));
        auto *page = new SessionEditorPopupPage(m_app.translator(), m_app.sessions(), m_app.daemonClient(), dialog);
        dialog->setContentWidget(page);
        dialog->resize(480, 280);
        connect(page, &SessionEditorPopupPage::saved, dialog, [dialog, this] {
            dialog->close();
            dialog->deleteLater();
            m_app.syncCoordinator().start();
        });
        connect(page, &SessionEditorPopupPage::cancelled, dialog, [dialog] {
            dialog->close();
            dialog->deleteLater();
        });
        dialog->showCentered();
    });

    wireSync();
    applyLocalizedUi();

#if defined(AVAR_GUI_HOSTING_DESKTOP)
    if (m_shellWindow != nullptr) {
        m_shellWindow->setChromeRadius(app.themeManager().currentTokens().radiusPx);
        connect(&app.themeManager(), &ThemeManager::themeChanged, this, [this](const ThemeTokens &tokens) {
            if (m_shellWindow != nullptr) {
                m_shellWindow->setChromeRadius(tokens.radiusPx);
            }
        });
    }
#endif

    auto *statsTimer = new QTimer(this);
    statsTimer->setInterval(3000);
    connect(statsTimer, &QTimer::timeout, this, [this] {
        m_app.daemonClient().systemStats([this](bool ok, const SystemStatsInfo &stats) {
            m_dashboard->setStats(stats, ok);
        });
        m_app.daemonClient().health([this](bool ok, const HealthInfo &health) {
            m_dashboard->setHealth(health, ok);
        });
    });
    statsTimer->start();

    GuiLog::instance().info(QStringLiteral("Avar Qt GUI started"));

#if defined(AVAR_GUI_HOSTING_DESKTOP)
    m_tray = std::make_unique<DesktopTray>(*this, m_app.translator(), m_app.daemonClient(), m_app.settings());
    m_tray->attachShellWindow(m_shellWindow.get());
    m_tray->show();
#endif
}

#if defined(AVAR_GUI_HOSTING_DESKTOP)
QMainWindow *MainWindow::shellWindow() const
{
    return m_shellWindow.get();
}
#endif

void MainWindow::show()
{
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    if (m_shellWindow) {
        m_shellWindow->show();
        return;
    }
#endif
    QWidget::show();
}

void MainWindow::applyLocalizedUi()
{
    applyApplicationLocale(m_app, m_app.translator());
    m_app.themeManager().syncStylesheet();
    if (m_shell != nullptr) {
        m_shell->retranslateUi();
        if (SessionSelector *sessions = m_shell->sessionSelector()) {
            sessions->retranslateUi();
        }
    }
    if (m_downloads != nullptr) {
        m_downloads->retranslateUi();
    }
    if (m_dashboard != nullptr) {
        m_dashboard->retranslateUi();
    }
    if (m_settings != nullptr) {
        m_settings->reloadLocalizedContent();
    }
    if (m_help != nullptr) {
        m_help->reloadContent();
    }
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    if (m_tray != nullptr) {
        m_tray->retranslateUi();
    }
#endif
}

void MainWindow::wireSync()
{
    m_shell->setConnectionState(m_app.syncCoordinator().connectionState());

    connect(&m_app.syncCoordinator(), &SyncCoordinator::connectionStateChanged, m_shell,
            [this](ConnectionState state) { m_shell->setConnectionState(state); });

    connect(&m_app.syncCoordinator(), &SyncCoordinator::snapshotReceived, this,
            [this](const SnapshotPayload &payload) {
                m_downloads->setDownloads(payload.downloads);
                m_queues->setQueues(payload.queues);
                m_shell->queuePanel()->setQueues(payload.queues);
                if (payload.hasHealth) {
                    m_dashboard->setHealth(payload.health, true);
                }
                if (payload.hasStats) {
                    m_dashboard->setStats(payload.stats, true);
                }
#if defined(AVAR_GUI_HOSTING_DESKTOP)
                if (m_tray) {
                    m_tray->updateFromDownloads(payload.downloads);
                }
#endif
            });
}

} // namespace avar::gui
