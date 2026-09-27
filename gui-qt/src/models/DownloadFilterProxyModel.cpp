#include "models/DownloadFilterProxyModel.hpp"

#include "models/DownloadTableModel.hpp"

namespace avar::gui {

DownloadFilterProxyModel::DownloadFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void DownloadFilterProxyModel::setQueueFilter(const QString &queueId)
{
    m_queueId = queueId;
    invalidateFilter();
}

void DownloadFilterProxyModel::setSearchText(const QString &text)
{
    m_search = text.trimmed();
    invalidateFilter();
}

bool DownloadFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex nameIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const QModelIndex statusIndex = sourceModel()->index(sourceRow, 1, sourceParent);
    const QModelIndex idIndex = sourceModel()->index(sourceRow, 3, sourceParent);

    if (!m_queueId.isEmpty()) {
        const auto *table = qobject_cast<const DownloadTableModel *>(sourceModel());
        if (table) {
            const DownloadInfo info = table->downloadAt(sourceRow);
            if (!info.queueId.isEmpty() && info.queueId != m_queueId) {
                return false;
            }
        }
    }

    if (m_search.isEmpty()) {
        return true;
    }
    const QString haystack =
        nameIndex.data().toString() + statusIndex.data().toString() + idIndex.data().toString();
    return haystack.contains(m_search, Qt::CaseInsensitive);
}

} // namespace avar::gui
