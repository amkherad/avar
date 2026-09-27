#include "models/DownloadTableModel.hpp"

namespace avar::gui {

DownloadTableModel::DownloadTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void DownloadTableModel::setDownloads(const QVector<DownloadInfo> &downloads)
{
    beginResetModel();
    m_rows = downloads;
    endResetModel();
}

DownloadInfo DownloadTableModel::downloadAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    return m_rows.at(row);
}

int DownloadTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int DownloadTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 4;
}

QVariant DownloadTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) {
        return {};
    }
    const DownloadInfo &row = m_rows.at(index.row());
    switch (index.column()) {
    case 0:
        return row.name;
    case 1:
        return row.status;
    case 2:
        return row.progress > 0 ? QString::number(row.progress * 100.0, 'f', 1) + QLatin1Char('%') : QString();
    case 3:
        return row.id;
    default:
        return {};
    }
}

QVariant DownloadTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case 0:
        return QStringLiteral("Name");
    case 1:
        return QStringLiteral("Status");
    case 2:
        return QStringLiteral("Progress");
    case 3:
        return QStringLiteral("ID");
    default:
        return {};
    }
}

} // namespace avar::gui
