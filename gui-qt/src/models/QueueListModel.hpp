#pragma once

#include "api/DaemonTypes.hpp"

#include <QAbstractListModel>

namespace avar::gui {

class QueueListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    explicit QueueListModel(QObject *parent = nullptr);

    void setQueues(const QVector<QueueInfo> &queues);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QVector<QueueInfo> m_rows;
};

} // namespace avar::gui
