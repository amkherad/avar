#include "ui/widgets/ResizeHandle.hpp"

#include <QMouseEvent>

namespace avar::gui {

ResizeHandle::ResizeHandle(ResizeAxis axis, QWidget *parent)
    : QWidget(parent)
    , m_axis(axis)
{
    setObjectName(QStringLiteral("AvarResizeHandle"));
    setProperty("axis", axis == ResizeAxis::Horizontal ? QStringLiteral("horizontal") : QStringLiteral("vertical"));
    setCursor(axis == ResizeAxis::Horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor);
    if (axis == ResizeAxis::Horizontal) {
        setMinimumWidth(6);
        setMaximumWidth(6);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    } else {
        setMinimumHeight(6);
        setMaximumHeight(6);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
}

void ResizeHandle::mousePressEvent(QMouseEvent *event)
{
    m_pressPos = event->globalPosition().toPoint();
}

void ResizeHandle::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint current = event->globalPosition().toPoint();
    const QPoint delta = current - m_pressPos;
    m_pressPos = current;
    if (m_axis == ResizeAxis::Horizontal) {
        emit resizeDelta(delta.x());
    } else {
        emit resizeDelta(delta.y());
    }
}

} // namespace avar::gui
