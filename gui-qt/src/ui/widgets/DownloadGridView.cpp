#include "ui/widgets/DownloadGridView.hpp"

#include <QListWidgetItem>
#include <QPainterPath>
#include <QRegion>
#include <QResizeEvent>
#include <QShowEvent>

namespace avar::gui {

namespace {

// Matches @@RADIUS_INNER@@ (theme radius − 2px) inside the 1px list border.
constexpr int kPanelCornerRadiusPx = 8;

} // namespace

DownloadGridView::DownloadGridView(QWidget *parent)
    : QListWidget(parent)
{
    setObjectName(QStringLiteral("AvarDownloadGrid"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);
    viewport()->setAttribute(Qt::WA_StyledBackground, true);
    viewport()->setAutoFillBackground(true);
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
    updateViewportClip();
}

void DownloadGridView::resizeEvent(QResizeEvent *event)
{
    QListWidget::resizeEvent(event);
    updateViewportClip();
}

void DownloadGridView::showEvent(QShowEvent *event)
{
    QListWidget::showEvent(event);
    updateViewportClip();
}

void DownloadGridView::updateViewportClip()
{
    QWidget *vp = viewport();
    if (vp == nullptr || vp->width() <= 0 || vp->height() <= 0) {
        return;
    }
    QPainterPath path;
    path.addRoundedRect(QRectF(vp->rect()), kPanelCornerRadiusPx, kPanelCornerRadiusPx);
    vp->setMask(QRegion(path.toFillPolygon().toPolygon()));
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
