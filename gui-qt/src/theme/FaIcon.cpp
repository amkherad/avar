#include "theme/FaIcon.hpp"

#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QTransform>

#if defined(AVAR_GUI_HAS_QT_SVG)
#include <QSvgRenderer>
#endif

namespace avar::gui {
namespace FaIcon {

namespace {

QString resourcePath(const QString &iconName)
{
    return QStringLiteral(":/fa/solid/%1.svg").arg(iconName);
}

#if defined(AVAR_GUI_HAS_QT_SVG)

bool renderSvgToPainter(QPainter &painter, const QByteArray &svgData)
{
    QSvgRenderer renderer(svgData);
    if (!renderer.isValid()) {
        return false;
    }
    renderer.render(&painter);
    return true;
}

QByteArray loadSvgResource(const QString &resource)
{
    QFile file(resource);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QPixmap tintedPixmapFromSvgData(const QByteArray &svgData, int size, const QColor &color)
{
    if (svgData.isEmpty()) {
        return {};
    }

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (!renderSvgToPainter(painter, svgData)) {
        return {};
    }
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    return pixmap;
}

#endif

QPixmap tintedPixmapFromIcon(const QString &resource, int size, const QColor &color)
{
    const QPixmap base = QIcon(resource).pixmap(QSize(size, size), QIcon::Normal, QIcon::Off);
    if (base.isNull()) {
        return {};
    }

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawPixmap(0, 0, base);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    return pixmap;
}

QPixmap mirrorPixmapHorizontally(const QPixmap &source)
{
    if (source.isNull()) {
        return {};
    }
    return source.transformed(QTransform().scale(-1.0, 1.0));
}

QPixmap tintedPixmap(const QString &resource, int size, const QColor &color)
{
#if defined(AVAR_GUI_HAS_QT_SVG)
    const QByteArray svgData = loadSvgResource(resource);
    QPixmap pixmap = tintedPixmapFromSvgData(svgData, size, color);
    if (!pixmap.isNull()) {
        return pixmap;
    }
#endif
    return tintedPixmapFromIcon(resource, size, color);
}

} // namespace

QIcon solid(const QString &iconName, int pixelSize, const QColor &color, SolidIconOptions options)
{
    const QString resource = resourcePath(iconName);
    QPixmap pixmap = tintedPixmap(resource, pixelSize, color);
    if (options.mirrorHorizontally) {
        pixmap = mirrorPixmapHorizontally(pixmap);
    }
    if (!pixmap.isNull()) {
        return QIcon(pixmap);
    }
    return QIcon(resource);
}

QIcon solidBack(int pixelSize, const QColor &color, bool layoutRtl)
{
    SolidIconOptions options;
    options.mirrorHorizontally = layoutRtl;
    return solid(QStringLiteral("arrow-left"), pixelSize, color, options);
}

} // namespace FaIcon
} // namespace avar::gui
