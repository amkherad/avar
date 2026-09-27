#pragma once

#include <QSortFilterProxyModel>

namespace avar::gui {

class DownloadFilterProxyModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit DownloadFilterProxyModel(QObject *parent = nullptr);

    void setQueueFilter(const QString &queueId);
    void setSearchText(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_queueId;
    QString m_search;
};

} // namespace avar::gui
