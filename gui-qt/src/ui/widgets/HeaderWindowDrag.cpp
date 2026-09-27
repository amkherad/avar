#include "ui/widgets/HeaderWindowDrag.hpp"

#include <QAbstractButton>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QWindow>

namespace avar::gui {

HeaderWindowDrag::HeaderWindowDrag(QWidget *header, QObject *parent)
    : QObject(parent)
    , m_header(header)
{
    if (m_header != nullptr) {
        m_header->installEventFilter(this);
    }
}

bool HeaderWindowDrag::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_header || m_header == nullptr) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && m_header->childAt(mouse->pos()) == nullptr) {
            QWidget *top = m_header->window();
            if (top != nullptr) {
                if (top->isMaximized()) {
                    top->showNormal();
                } else {
                    top->showMaximized();
                }
            }
            return true;
        }
    }

    if (event->type() != QEvent::MouseButtonPress) {
        return QObject::eventFilter(watched, event);
    }

    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton) {
        return QObject::eventFilter(watched, event);
    }

    QWidget *child = m_header->childAt(mouse->pos());
    if (child != nullptr) {
        for (QWidget *w = child; w != nullptr && w != m_header; w = w->parentWidget()) {
            if (qobject_cast<QAbstractButton *>(w) != nullptr) {
                return QObject::eventFilter(watched, event);
            }
        }
    }

    QWidget *top = m_header->window();
    if (top == nullptr || top->windowHandle() == nullptr) {
        return QObject::eventFilter(watched, event);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (top->windowHandle()->startSystemMove()) {
        return true;
    }
#endif
    return QObject::eventFilter(watched, event);
}

} // namespace avar::gui
