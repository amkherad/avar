#pragma once

#include "api/DaemonClient.hpp"
#include "config/LayoutPreferences.hpp"

#include <QWidget>

class QLineEdit;
class QStackedWidget;
class QTableView;

namespace avar::gui {

class Translator;
class DownloadTableModel;
class DownloadFilterProxyModel;
class DownloadGridView;
class ConsoleDock;
class FooterBar;
class DownloadDetailPanelWidget;

class DashboardPage final : public QWidget {
    Q_OBJECT

public:
    DashboardPage(Translator &translator,
                  LayoutPreferences &layout,
                  DaemonClient &daemon,
                  DownloadTableModel &downloads,
                  QWidget *parent = nullptr);

    void setQueueFilterId(const QString &queueId);
    void setHealth(const HealthInfo &health, bool valid);
    void setStats(const SystemStatsInfo &stats, bool valid);

    [[nodiscard]] ConsoleDock *consoleDock() const;

signals:
    void addDownloadRequested();
    void batchAddRequested();

private:
    void filterDownloadsForQueue();
    void onSelectionChanged();
    void refreshGridFromProxy();
    void applyViewMode();
    void showDownloadById(const QString &id);

    Translator &m_tr;
    LayoutPreferences &m_layout;
    DaemonClient &m_daemon;
    DownloadTableModel &m_downloads;
    DownloadFilterProxyModel *m_proxy = nullptr;
    QString m_queueId;
    FooterBar *m_footer = nullptr;
    ConsoleDock *m_console = nullptr;
    DownloadDetailPanelWidget *m_detail = nullptr;
    QStackedWidget *m_listStack = nullptr;
    QTableView *m_table = nullptr;
    DownloadGridView *m_grid = nullptr;
    QLineEdit *m_search = nullptr;
    qint64 m_logOffset = 0;
};

} // namespace avar::gui
