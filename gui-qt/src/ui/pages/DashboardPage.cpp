#include "ui/pages/DashboardPage.hpp"

#include "core/GuiLog.hpp"
#include "i18n/Translator.hpp"
#include "models/DownloadFilterProxyModel.hpp"
#include "models/DownloadTableModel.hpp"
#include "ui/widgets/AvarButton.hpp"
#include "ui/widgets/DownloadGridView.hpp"
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
#include <QTimer>
#include <QVBoxLayout>

namespace avar::gui {

DashboardPage::DashboardPage(Translator &translator,
                             LayoutPreferences &layout,
                             DaemonClient &daemon,
                             DownloadTableModel &downloads,
                             QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_layout(layout)
    , m_daemon(daemon)
    , m_downloads(downloads)
{
    setObjectName(QStringLiteral("AvarDashboard"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 0);
    root->setSpacing(8);

    auto *cardHeader = new QHBoxLayout();
    auto *title = new QLabel(m_tr.tr(QStringLiteral("download.title")), this);
    title->setProperty("class", QStringLiteral("AvarCardTitle"));
    cardHeader->addWidget(title);
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

    auto *batchBtn = new AvarButton(AvarButtonVariant::Secondary, this);
    batchBtn->setText(m_tr.tr(QStringLiteral("download.batchAdd.button")));
    connect(batchBtn, &QPushButton::clicked, this, &DashboardPage::batchAddRequested);

    auto *addBtn = new AvarButton(AvarButtonVariant::Primary, this);
    addBtn->setText(m_tr.tr(QStringLiteral("download.add")));
    connect(addBtn, &QPushButton::clicked, this, &DashboardPage::addDownloadRequested);

    cardHeader->addWidget(gridBtn);
    cardHeader->addWidget(listBtn);
    cardHeader->addWidget(batchBtn);
    cardHeader->addWidget(addBtn);
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
        m_layout.setConsoleOpen(!m_layout.consoleOpen());
    });
    root->addWidget(m_footer);

    m_console = new ConsoleDock(m_tr, layout, this);
    m_console->setFixedHeight(layout.consoleHeight());
    connect(&layout, &LayoutPreferences::layoutChanged, this, [this] {
        m_console->setFixedHeight(m_layout.consoleHeight());
        m_console->setOpen(m_layout.consoleOpen());
    });
    root->addWidget(m_console);

    connect(&GuiLog::instance(), &GuiLog::lineAppended, m_console, &ConsoleDock::appendLine);

    auto *logTimer = new QTimer(this);
    logTimer->setInterval(2000);
    connect(logTimer, &QTimer::timeout, this, [this] {
        if (!m_layout.consoleOpen()) {
            return;
        }
        m_daemon.getLogs(80, m_logOffset, [this](bool ok, const QString &logs, qint64 next) {
            if (!ok || logs.trimmed().isEmpty()) {
                return;
            }
            for (const QString &line : logs.split(QLatin1Char('\n'))) {
                if (!line.isEmpty()) {
                    m_console->appendLine(QStringLiteral("[daemon] %1").arg(line));
                }
            }
            m_logOffset = next;
        });
    });
    logTimer->start();

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

} // namespace avar::gui
