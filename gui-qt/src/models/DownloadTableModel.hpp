#pragma once

#include "api/DaemonTypes.hpp"

#include <QAbstractTableModel>

namespace avar::gui {

class Translator;

class DownloadTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit DownloadTableModel(Translator &translator, QObject *parent = nullptr);

    void setDownloads(const QVector<DownloadInfo> &downloads);
    void retranslateUi();
    [[nodiscard]] DownloadInfo downloadAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    Translator &m_tr;
    QVector<DownloadInfo> m_rows;
};

} // namespace avar::gui
