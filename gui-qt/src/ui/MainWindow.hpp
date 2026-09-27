#pragma once

#include "ui/AppShell.hpp"

#include <QWidget>
#include <memory>

namespace avar::gui {

class Application;
class DownloadTableModel;
class QueueListModel;
class Translator;
class DesktopShellWindow;

class MainWindow final : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(Application &app, QWidget *parent = nullptr);
    ~MainWindow() override;

    void show();

private:
    void wireSync();

    Application &m_app;
    std::unique_ptr<Translator> m_tr;
    std::unique_ptr<DownloadTableModel> m_downloads;
    std::unique_ptr<QueueListModel> m_queues;
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    std::unique_ptr<DesktopShellWindow> m_shellWindow;
#endif
    AppShell *m_shell = nullptr;
    class DashboardPage *m_dashboard = nullptr;
    class SettingsPage *m_settings = nullptr;
    class HelpPage *m_help = nullptr;
};

} // namespace avar::gui
