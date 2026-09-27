#pragma once

#include "ui/AppShell.hpp"

#include <QWidget>

#include <memory>

class QMainWindow;

namespace avar::gui {

class Application;
class DownloadTableModel;
class QueueListModel;
class DesktopShellWindow;
class DesktopTray;

class MainWindow final : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(Application &app, QWidget *parent = nullptr);
    ~MainWindow() override;

    void show();

#if defined(AVAR_GUI_HOSTING_DESKTOP)
    [[nodiscard]] QMainWindow *shellWindow() const;
#endif

private:
    void wireSync();
    void applyLocalizedUi();

    Application &m_app;
    std::unique_ptr<DownloadTableModel> m_downloads;
    std::unique_ptr<QueueListModel> m_queues;
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    std::unique_ptr<DesktopShellWindow> m_shellWindow;
    std::unique_ptr<DesktopTray> m_tray;
#endif
    AppShell *m_shell = nullptr;
    class DashboardPage *m_dashboard = nullptr;
    class SettingsPage *m_settings = nullptr;
    class HelpPage *m_help = nullptr;
};

} // namespace avar::gui
