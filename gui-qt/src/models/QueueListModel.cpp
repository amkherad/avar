#include "models/QueueListModel.hpp"

namespace avar::gui {

QueueListModel::QueueListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void QueueListModel::setQueues(const QVector<QueueInfo> &queues)
{
    beginResetModel();
    m_rows = queues;
    endResetModel();
}

int QueueListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant QueueListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) {
        return {};
    }
    return m_rows.at(index.row()).name;
}

} // namespace avar::gui
