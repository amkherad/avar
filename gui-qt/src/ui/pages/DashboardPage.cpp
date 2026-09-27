#include "ui/pages/DashboardPage.hpp"

#include "core/GuiLog.hpp"
#include "i18n/Translator.hpp"
#include "models/DownloadFilterProxyModel.hpp"
#include "models/DownloadTableModel.hpp"
#include "ui/widgets/AvarButton.hpp"
#include "ui/widgets/DownloadGridView.hpp"
#include "console/ConsoleStore.hpp"
#include "sync/SyncCoordinator.hpp"
#include "ui/widgets/ConsoleDock.hpp"
#include "ui/widgets/DownloadDetailPanelWidget.hpp"
#include "ui/widgets/FooterBar.hpp"
#include "ui/widgets/ResizeHandle.hpp"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace avar::gui {

DashboardPage::DashboardPage(Translator &translator,
                             LayoutPreferences &layout,
                             DaemonClient &daemon,
                             SyncCoordinator &sync,
                             DownloadTableModel &downloads,
                             QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_layout(layout)
    , m_daemon(daemon)
    , m_downloads(downloads)
    , m_consoleStore(this)
{
    setObjectName(QStringLiteral("AvarDashboard"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 0);
    root->setSpacing(6);

    auto *cardHeader = new QHBoxLayout();
    m_titleLabel = new QLabel(m_tr.tr(QStringLiteral("download.title")), this);
    m_titleLabel->setProperty("class", QStringLiteral("AvarCardTitle"));
    cardHeader->addWidget(m_titleLabel);
    cardHeader->addStretch();

    auto *gridBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    gridBtn->setText(QStringLiteral("▦"));
    gridBtn->setToolTip(QStringLiteral("Grid"));
    auto *listBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    listBtn->setText(QStringLiteral("☰"));
    listBtn->setToolTip(QStringLiteral("List"));
    connect(gridBtn, &QPushButton::clicked, this, [this] {
        m_layout.setDownloadViewMode(DownloadViewMode::Grid);
        applyViewMode();
    });
    connect(listBtn, &QPushButton::clicked, this, [this] {
        m_layout.setDownloadViewMode(DownloadViewMode::Compact);
        applyViewMode();
    });
    connect(&m_layout, &LayoutPreferences::layoutChanged, this, &DashboardPage::applyViewMode);

    m_batchBtn = new AvarButton(AvarButtonVariant::Secondary, this);
    m_batchBtn->setText(m_tr.tr(QStringLiteral("download.batchAdd.button")));
    connect(m_batchBtn, &QPushButton::clicked, this, &DashboardPage::batchAddRequested);

    m_addBtn = new AvarButton(AvarButtonVariant::Primary, this);
    m_addBtn->setText(m_tr.tr(QStringLiteral("download.add")));
    connect(m_addBtn, &QPushButton::clicked, this, &DashboardPage::addDownloadRequested);

    cardHeader->addWidget(gridBtn);
    cardHeader->addWidget(listBtn);
    cardHeader->addWidget(m_batchBtn);
    cardHeader->addWidget(m_addBtn);
    root->addLayout(cardHeader);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(m_tr.tr(QStringLiteral("download.searchPlaceholder")));
    root->addWidget(m_search);

    m_proxy = new DownloadFilterProxyModel(this);
    m_proxy->setSourceModel(&m_downloads);
    connect(m_search, &QLineEdit::textChanged, m_proxy, &DownloadFilterProxyModel::setSearchText);
    connect(m_proxy, &QAbstractItemModel::modelReset, this, &DashboardPage::refreshGridFromProxy);
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, &DashboardPage::refreshGridFromProxy);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, &DashboardPage::refreshGridFromProxy);
    connect(m_proxy, &QAbstractItemModel::dataChanged, this, &DashboardPage::refreshGridFromProxy);

    auto *workspace = new QHBoxLayout();
    workspace->setSpacing(0);

    auto *center = new QVBoxLayout();
    m_listStack = new QStackedWidget(this);
    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { onSelectionChanged(); });
    m_grid = new DownloadGridView(this);
    connect(m_grid, &DownloadGridView::downloadActivated, this, &DashboardPage::showDownloadById);
    m_listStack->addWidget(m_table);
    m_listStack->addWidget(m_grid);
    center->addWidget(m_listStack, 1);
    applyViewMode();

    m_detail = new DownloadDetailPanelWidget(m_tr, this);
    m_detail->setFixedWidth(m_layout.detailPanelWidth());
    auto *detailResize = new ResizeHandle(ResizeAxis::Horizontal, this);
    connect(detailResize, &ResizeHandle::resizeDelta, &m_layout, &LayoutPreferences::adjustDetailPanelWidth);
    connect(&m_layout, &LayoutPreferences::layoutChanged, this, [this] {
        m_detail->setVisible(m_layout.detailPanelOpen());
        m_detail->setFixedWidth(m_layout.detailPanelWidth());
    });

    workspace->addLayout(center, 1);
    workspace->addWidget(detailResize);
    workspace->addWidget(m_detail);
    root->addLayout(workspace, 1);

    m_footer = new FooterBar(m_tr, this);
    connect(m_footer, &FooterBar::consoleToggleRequested, this, [this] {
        const bool next = !m_layout.consoleOpen();
        m_layout.setConsoleOpen(next);
        if (next) {
            m_consoleStore.markErrorsSeen();
        }
    });
    const auto refreshConsoleFooter = [this] {
        m_footer->setConsoleButtonState(m_layout.consoleOpen(), m_consoleStore.hasUnseenErrors());
    };
    connect(&m_consoleStore, &ConsoleStore::changed, this, refreshConsoleFooter);
    connect(&m_layout, &LayoutPreferences::layoutChanged, this, refreshConsoleFooter);
    refreshConsoleFooter();
    root->addWidget(m_footer);

    m_console = new ConsoleDock(m_tr, layout, m_consoleStore, m_daemon, sync, this);
    m_console->setFixedHeight(layout.consoleHeight());
    connect(&layout, &LayoutPreferences::layoutChanged, this, [this] {
        m_console->setFixedHeight(m_layout.consoleHeight());
        m_console->setOpen(m_layout.consoleOpen());
    });
    root->addWidget(m_console);

    connect(&GuiLog::instance(), &GuiLog::lineAppended, &m_consoleStore, &ConsoleStore::appendGuiLogLine);

    m_detail->setVisible(m_layout.detailPanelOpen());
}

void DashboardPage::setQueueFilterId(const QString &queueId)
{
    m_queueId = queueId;
    filterDownloadsForQueue();
}

void DashboardPage::setHealth(const HealthInfo &health, bool valid)
{
    m_footer->setHealth(health, valid);
}

void DashboardPage::setStats(const SystemStatsInfo &stats, bool valid)
{
    m_footer->setStats(stats, valid);
}

ConsoleDock *DashboardPage::consoleDock() const
{
    return m_console;
}

void DashboardPage::filterDownloadsForQueue()
{
    if (m_proxy) {
        m_proxy->setQueueFilter(m_queueId);
        refreshGridFromProxy();
    }
}

void DashboardPage::refreshGridFromProxy()
{
    if (!m_grid || !m_proxy) {
        return;
    }
    QVector<DownloadInfo> visible;
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QModelIndex proxyIndex = m_proxy->index(row, 0);
        const int sourceRow = m_proxy->mapToSource(proxyIndex).row();
        visible.push_back(m_downloads.downloadAt(sourceRow));
    }
    m_grid->setDownloads(visible);
}

void DashboardPage::applyViewMode()
{
    if (!m_listStack) {
        return;
    }
    const bool grid = m_layout.downloadViewMode() == DownloadViewMode::Grid;
    m_listStack->setCurrentWidget(grid ? static_cast<QWidget *>(m_grid) : m_table);
}

void DashboardPage::showDownloadById(const QString &id)
{
    for (int row = 0; row < m_downloads.rowCount(); ++row) {
        const DownloadInfo info = m_downloads.downloadAt(row);
        if (info.id == id) {
            m_layout.setDetailPanelOpen(true);
            m_detail->setDownload(info, true);
            return;
        }
    }
}

void DashboardPage::onSelectionChanged()
{
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        m_layout.setDetailPanelOpen(false);
        m_detail->setDownload({}, false);
        return;
    }
    m_layout.setDetailPanelOpen(true);
    const int proxyRow = rows.first().row();
    const int sourceRow = m_proxy->mapToSource(m_proxy->index(proxyRow, 0)).row();
    const DownloadInfo info = m_downloads.downloadAt(sourceRow);
    m_detail->setDownload(info, true);
}

void DashboardPage::retranslateUi()
{
    if (m_titleLabel != nullptr) {
        m_titleLabel->setText(m_tr.tr(QStringLiteral("download.title")));
    }
    if (m_batchBtn != nullptr) {
        m_batchBtn->setText(m_tr.tr(QStringLiteral("download.batchAdd.button")));
    }
    if (m_addBtn != nullptr) {
        m_addBtn->setText(m_tr.tr(QStringLiteral("download.add")));
    }
    if (m_search != nullptr) {
        m_search->setPlaceholderText(m_tr.tr(QStringLiteral("download.searchPlaceholder")));
    }
    if (m_footer != nullptr) {
        m_footer->retranslateUi();
    }
    if (m_console != nullptr) {
        m_console->retranslateUi();
    }
    if (m_detail != nullptr) {
        m_detail->retranslateUi();
    }
}

} // namespace avar::gui
