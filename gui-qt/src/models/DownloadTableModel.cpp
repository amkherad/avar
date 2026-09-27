#include "models/DownloadTableModel.hpp"

#include "i18n/Translator.hpp"

namespace avar::gui {

DownloadTableModel::DownloadTableModel(Translator &translator, QObject *parent)
    : QAbstractTableModel(parent)
    , m_tr(translator)
{
}

void DownloadTableModel::retranslateUi()
{
    emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
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
        return m_tr.tr(QStringLiteral("download.filename"));
    case 1:
        return m_tr.tr(QStringLiteral("download.status"));
    case 2:
        return m_tr.tr(QStringLiteral("download.progress"));
    case 3:
        return m_tr.tr(QStringLiteral("download.id"));
    default:
        return {};
    }
}

} // namespace avar::gui
