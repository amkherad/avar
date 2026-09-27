#include "ui/DesktopTray.hpp"

#include "api/DaemonClient.hpp"
#include "config/AppSettings.hpp"
#include "i18n/Translator.hpp"
#include "ui/MainWindow.hpp"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>

#include <algorithm>

namespace avar::gui {

namespace {

constexpr int kMaxTrayDownloads = 3;

QString truncateFilename(const QString &name, int maxLen = 48)
{
    if (name.size() <= maxLen) {
        return name;
    }
    return name.left(maxLen - 1) + QStringLiteral("…");
}

} // namespace

DesktopTray::DesktopTray(MainWindow &window,
                         Translator &translator,
                         DaemonClient &daemon,
                         AppSettings &settings,
                         QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_tr(translator)
    , m_daemon(daemon)
    , m_settings(settings)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    QApplication::setQuitOnLastWindowClosed(false);

    const QIcon icon(QStringLiteral(":/icon.svg"));
    m_tray = new QSystemTrayIcon(icon.pixmap(16, 16), this);
    m_tray->setToolTip(QStringLiteral("Avar"));

    m_menu = new QMenu();
    connect(m_menu, &QMenu::aboutToShow, this, &DesktopTray::rebuildMenu);
#if !defined(Q_OS_LINUX)
    m_tray->setContextMenu(m_menu);
#endif

    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Context) {
            showContextMenu();
            return;
        }
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick
            || reason == QSystemTrayIcon::MiddleClick) {
            showMainWindow();
        }
    });

    connect(&m_settings, &AppSettings::desktopBehaviorChanged, this,
            &DesktopTray::applyQuitOnLastWindowClosed);
    connect(&m_settings, &AppSettings::localeChanged, this, [this] {
        updateTooltip();
    });

    applyQuitOnLastWindowClosed();
#endif
}

DesktopTray::~DesktopTray() = default;

void DesktopTray::show()
{
    if (m_tray) {
        m_tray->show();
    }
}

void DesktopTray::attachShellWindow(QMainWindow *shellWindow)
{
    if (m_shellWindow) {
        m_shellWindow->removeEventFilter(this);
    }
    m_shellWindow = shellWindow;
    if (m_shellWindow) {
        m_shellWindow->installEventFilter(this);
    }
}

void DesktopTray::setActiveDownloads(const QVector<TrayDownloadItem> &items)
{
    m_activeDownloads = items;
    updateTooltip();
}

void DesktopTray::updateFromDownloads(const QVector<DownloadInfo> &downloads)
{
    QVector<TrayDownloadItem> active;
    for (const DownloadInfo &item : downloads) {
        if (item.status != QStringLiteral("downloading")) {
            continue;
        }
        TrayDownloadItem row;
        row.id = item.id;
        row.filename = item.name;
        row.percent = progressPercent(item);
        active.push_back(row);
    }
    std::ranges::sort(active, [](const TrayDownloadItem &a, const TrayDownloadItem &b) {
        return a.id < b.id;
    });
    if (active.size() > kMaxTrayDownloads) {
        active.resize(kMaxTrayDownloads);
    }
    setActiveDownloads(active);
}

bool DesktopTray::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_appIsQuitting && watched == m_shellWindow && event->type() == QEvent::Close) {
        if (m_settings.keepInTrayOnClose() && m_tray) {
            event->ignore();
            m_shellWindow->hide();
            return true;
        }
        m_appIsQuitting = true;
    }
    return QObject::eventFilter(watched, event);
}

void DesktopTray::showMainWindow()
{
    m_window.show();
    if (!m_shellWindow) {
        return;
    }
    if (m_shellWindow->isMinimized()) {
        m_shellWindow->showNormal();
    }
    m_shellWindow->show();
    m_shellWindow->raise();
    m_shellWindow->activateWindow();
}

void DesktopTray::applyQuitOnLastWindowClosed()
{
    const bool keepTray = m_settings.keepInTrayOnClose() && m_tray != nullptr;
    QApplication::setQuitOnLastWindowClosed(!keepTray);
}

void DesktopTray::showContextMenu()
{
    if (!m_menu || !m_tray) {
        return;
    }
    m_menu->popup(QCursor::pos());
}

void DesktopTray::retranslateUi()
{
    updateTooltip();
    rebuildMenu();
}

void DesktopTray::rebuildMenu()
{
    if (!m_menu) {
        return;
    }
    m_menu->clear();

    m_menu->addAction(m_tr.tr(QStringLiteral("tray.show")), this, &DesktopTray::showMainWindow);
    m_menu->addSeparator();

    if (!m_activeDownloads.isEmpty()) {
        auto *heading = m_menu->addAction(m_tr.tr(QStringLiteral("tray.activeDownloads")));
        heading->setEnabled(false);
        for (const TrayDownloadItem &item : m_activeDownloads) {
            const QString label =
                QStringLiteral("%1 (%2%)").arg(truncateFilename(item.filename), QString::number(item.percent));
            auto *row = m_menu->addAction(label);
            row->setEnabled(false);
        }
        m_menu->addSeparator();
    }

    m_menu->addAction(m_tr.tr(QStringLiteral("tray.startAll")), this, [this] { runBulkAction(QStringLiteral("start")); });
    m_menu->addAction(m_tr.tr(QStringLiteral("tray.pauseAll")), this, [this] { runBulkAction(QStringLiteral("pause")); });
    m_menu->addAction(m_tr.tr(QStringLiteral("tray.resumeAll")), this, [this] { runBulkAction(QStringLiteral("resume")); });
    m_menu->addAction(m_tr.tr(QStringLiteral("tray.stopAll")), this, [this] { runBulkAction(QStringLiteral("stop")); });
    m_menu->addSeparator();
    m_menu->addAction(m_tr.tr(QStringLiteral("tray.exit")), this, [this] {
        m_appIsQuitting = true;
        if (m_tray) {
            m_tray->hide();
        }
        qApp->quit();
    });
}

void DesktopTray::updateTooltip()
{
    if (!m_tray) {
        return;
    }
    QString tooltip = QStringLiteral("Avar");
    if (!m_activeDownloads.isEmpty()) {
        QStringList lines;
        for (const TrayDownloadItem &item : m_activeDownloads) {
            lines.append(QStringLiteral("%1 (%2%)").arg(item.filename, QString::number(item.percent)));
        }
        tooltip = QStringLiteral("Avar\n%1").arg(lines.join(QLatin1Char('\n')));
    }
    if (tooltip == m_lastTooltip) {
        return;
    }
    m_lastTooltip = tooltip;
    m_tray->setToolTip(tooltip);
}

void DesktopTray::runBulkAction(const QString &kind)
{
    m_daemon.listDownloads([this, kind](bool ok, QVector<DownloadInfo> downloads, QString) {
        if (!ok) {
            return;
        }
        m_allDownloads = std::move(downloads);
        for (const DownloadInfo &item : m_allDownloads) {
            const QString &status = item.status;
            bool match = false;
            if (kind == QStringLiteral("start")) {
                match = canStart(status);
            } else if (kind == QStringLiteral("pause")) {
                match = canPause(status);
            } else if (kind == QStringLiteral("resume")) {
                match = canResume(status);
            } else if (kind == QStringLiteral("stop")) {
                match = canStop(status);
            }
            if (!match || item.id.isEmpty()) {
                continue;
            }
            if (kind == QStringLiteral("start")) {
                m_daemon.startDownload(item.id, [](bool, const QString &) {});
            } else if (kind == QStringLiteral("pause")) {
                m_daemon.pauseDownload(item.id, [](bool, const QString &) {});
            } else if (kind == QStringLiteral("resume")) {
                m_daemon.resumeDownload(item.id, [](bool, const QString &) {});
            } else if (kind == QStringLiteral("stop")) {
                m_daemon.stopDownload(item.id, [](bool, const QString &) {});
            }
        }
    });
}

int DesktopTray::progressPercent(const DownloadInfo &item)
{
    if (item.progress > 0) {
        return qBound(0, static_cast<int>(qRound(item.progress * 100.0)), 100);
    }
    if (item.totalBytes > 0) {
        return qBound(0,
                      static_cast<int>(qRound(100.0 * static_cast<double>(item.downloadedBytes)
                                              / static_cast<double>(item.totalBytes))),
                      100);
    }
    return 0;
}

bool DesktopTray::canStart(const QString &status)
{
    return status == QStringLiteral("queued") || status == QStringLiteral("stopped")
           || status == QStringLiteral("error") || status == QStringLiteral("failed")
           || status == QStringLiteral("cancelled");
}

bool DesktopTray::canPause(const QString &status)
{
    return status == QStringLiteral("downloading");
}

bool DesktopTray::canResume(const QString &status)
{
    return status == QStringLiteral("paused");
}

bool DesktopTray::canStop(const QString &status)
{
    return status == QStringLiteral("downloading") || status == QStringLiteral("paused")
           || status == QStringLiteral("queued");
}

} // namespace avar::gui
