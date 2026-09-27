#pragma once

#include "api/DaemonTypes.hpp"

#include <QListWidget>

class QResizeEvent;
class QShowEvent;

namespace avar::gui {

class DownloadGridView final : public QListWidget {
    Q_OBJECT

public:
    explicit DownloadGridView(QWidget *parent = nullptr);

    void setDownloads(const QVector<DownloadInfo> &items);

signals:
    void downloadActivated(const QString &id);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void updateViewportClip();
    void rebuild();
    QVector<DownloadInfo> m_items;
};

} // namespace avar::gui
