#pragma once

#include "api/DaemonTypes.hpp"

#include <QListWidget>

namespace avar::gui {

class DownloadGridView final : public QListWidget {
    Q_OBJECT

public:
    explicit DownloadGridView(QWidget *parent = nullptr);

    void setDownloads(const QVector<DownloadInfo> &items);

signals:
    void downloadActivated(const QString &id);

private:
    void rebuild();
    QVector<DownloadInfo> m_items;
};

} // namespace avar::gui
