#pragma once

#include "api/DaemonTypes.hpp"

#include <QObject>
#include <QVector>

class QMainWindow;
class QMenu;
class QSystemTrayIcon;

namespace avar::gui {

class AppSettings;
class DaemonClient;
class MainWindow;
class Translator;

struct TrayDownloadItem {
    QString id;
    QString filename;
    int percent = 0;
};

class DesktopTray final : public QObject {
    Q_OBJECT

public:
    DesktopTray(MainWindow &window,
                Translator &translator,
                DaemonClient &daemon,
                AppSettings &settings,
                QObject *parent = nullptr);
    ~DesktopTray() override;

    void show();
    void attachShellWindow(QMainWindow *shellWindow);
    void setActiveDownloads(const QVector<TrayDownloadItem> &items);
    void updateFromDownloads(const QVector<DownloadInfo> &downloads);
    void retranslateUi();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildMenu();
    void showContextMenu();
    void updateTooltip();
    void showMainWindow();
    void runBulkAction(const QString &kind);
    void applyQuitOnLastWindowClosed();

    static int progressPercent(const DownloadInfo &item);
    static bool canStart(const QString &status);
    static bool canPause(const QString &status);
    static bool canResume(const QString &status);
    static bool canStop(const QString &status);

    MainWindow &m_window;
    Translator &m_tr;
    DaemonClient &m_daemon;
    AppSettings &m_settings;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QMainWindow *m_shellWindow = nullptr;
    QVector<TrayDownloadItem> m_activeDownloads;
    QVector<DownloadInfo> m_allDownloads;
    bool m_appIsQuitting = false;
    QString m_lastTooltip;
};

} // namespace avar::gui
