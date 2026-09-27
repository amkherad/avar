#include "ui/widgets/DownloadGridView.hpp"

#include <QListWidgetItem>

namespace avar::gui {

DownloadGridView::DownloadGridView(QWidget *parent)
    : QListWidget(parent)
{
    setObjectName(QStringLiteral("AvarDownloadGrid"));
    setViewMode(QListView::IconMode);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setSpacing(8);
    setUniformItemSizes(true);
    setWordWrap(true);
    setIconSize(QSize(48, 48));
    connect(this, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item) {
            emit downloadActivated(item->data(Qt::UserRole).toString());
        }
    });
    connect(this, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            emit downloadActivated(item->data(Qt::UserRole).toString());
        }
    });
}

void DownloadGridView::setDownloads(const QVector<DownloadInfo> &items)
{
    m_items = items;
    rebuild();
}

void DownloadGridView::rebuild()
{
    clear();
    for (const DownloadInfo &info : m_items) {
        const QString subtitle = info.progress > 0
                                   ? QString::number(info.progress * 100.0, 'f', 0) + QLatin1Char('%')
                                   : info.status;
        auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(info.name, subtitle));
        item->setData(Qt::UserRole, info.id);
        item->setSizeHint(QSize(140, 88));
        addItem(item);
    }
}

} // namespace avar::gui
