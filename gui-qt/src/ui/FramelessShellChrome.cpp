#include "ui/FramelessShellChrome.hpp"

#include <QBitmap>
#include <QEvent>
#include <QList>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWidget>
#include <QStyle>
#include <QWindow>

namespace avar::gui {

namespace {

void runLayoutChangeCallback(const std::function<void()> &callback)
{
    if (callback) {
        callback();
    }
}

} // namespace

bool windowIsMaximized(const QMainWindow &window)
{
    return (window.windowState() & Qt::WindowMaximized) != 0;
}

void polishDynamicFlag(QWidget *widget, const char *name, bool value)
{
    if (widget == nullptr) {
        return;
    }
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void applyFramelessPresentation(QMainWindow &window, bool maximized)
{
    window.setAttribute(Qt::WA_TranslucentBackground, !maximized);
    polishDynamicFlag(&window, "maximized", maximized);
    if (maximized) {
        window.clearMask();
    }
}

namespace {

constexpr int kResizeGrip = 8;
constexpr char kResizeEdgesProperty[] = "avarResizeEdges";

Qt::CursorShape cursorForEdges(Qt::Edges edges)
{
    const bool left = edges.testFlag(Qt::LeftEdge);
    const bool right = edges.testFlag(Qt::RightEdge);
    const bool top = edges.testFlag(Qt::TopEdge);
    const bool bottom = edges.testFlag(Qt::BottomEdge);
    if (left && top) {
        return Qt::SizeFDiagCursor;
    }
    if (right && bottom) {
        return Qt::SizeFDiagCursor;
    }
    if (right && top) {
        return Qt::SizeBDiagCursor;
    }
    if (left && bottom) {
        return Qt::SizeBDiagCursor;
    }
    if (left || right) {
        return Qt::SizeHorCursor;
    }
    if (top || bottom) {
        return Qt::SizeVerCursor;
    }
    return Qt::ArrowCursor;
}

void applyRoundedMask(QWidget *frame, int radiusPx)
{
    if (frame == nullptr || radiusPx <= 0) {
        return;
    }
    const QSize size = frame->size();
    if (size.isEmpty()) {
        return;
    }

    QBitmap bitmap(size);
    bitmap.fill(Qt::color0);
    QPainter painter(&bitmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::color1);
    painter.drawRoundedRect(QRect(QPoint(0, 0), size), radiusPx, radiusPx);
    frame->setMask(bitmap);
}

class ResizeGrip final : public QWidget {
public:
    ResizeGrip(Qt::Edges edges, QWidget *parent)
        : QWidget(parent)
        , m_edges(edges)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setAutoFillBackground(false);
        setCursor(cursorForEdges(edges));
        setProperty(kResizeEdgesProperty, static_cast<int>(edges));
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            return;
        }
        QWidget *top = window();
        if (top == nullptr || top->isMaximized()) {
            return;
        }
        QWindow *handle = top->windowHandle();
        if (handle == nullptr) {
            return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (handle->startSystemResize(m_edges)) {
            event->accept();
            return;
        }
#endif
        m_manualResize = true;
        m_pressGlobal = event->globalPosition().toPoint();
        m_pressGeometry = top->geometry();
        grabMouse();
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_manualResize) {
            return;
        }
        QWidget *top = window();
        if (top == nullptr) {
            return;
        }
        const QPoint delta = event->globalPosition().toPoint() - m_pressGlobal;
        QRect geometry = m_pressGeometry;
        if (m_edges.testFlag(Qt::LeftEdge)) {
            geometry.setLeft(geometry.left() + delta.x());
        }
        if (m_edges.testFlag(Qt::RightEdge)) {
            geometry.setRight(geometry.right() + delta.x());
        }
        if (m_edges.testFlag(Qt::TopEdge)) {
            geometry.setTop(geometry.top() + delta.y());
        }
        if (m_edges.testFlag(Qt::BottomEdge)) {
            geometry.setBottom(geometry.bottom() + delta.y());
        }
        const QSize minSize = top->minimumSize();
        if (geometry.width() < minSize.width()) {
            if (m_edges.testFlag(Qt::LeftEdge)) {
                geometry.setLeft(geometry.right() - minSize.width() + 1);
            } else {
                geometry.setRight(geometry.left() + minSize.width() - 1);
            }
        }
        if (geometry.height() < minSize.height()) {
            if (m_edges.testFlag(Qt::TopEdge)) {
                geometry.setTop(geometry.bottom() - minSize.height() + 1);
            } else {
                geometry.setBottom(geometry.top() + minSize.height() - 1);
            }
        }
        top->setGeometry(geometry);
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_manualResize && event->button() == Qt::LeftButton) {
            m_manualResize = false;
            releaseMouse();
            event->accept();
        }
    }

private:
    Qt::Edges m_edges;
    bool m_manualResize = false;
    QPoint m_pressGlobal;
    QRect m_pressGeometry;
};

class ChromeSyncFilter final : public QObject {
public:
    ChromeSyncFilter(FramelessShellChrome &chrome, QMainWindow &window, QWidget *frame)
        : QObject(&window)
        , m_chrome(chrome)
        , m_window(window)
        , m_frame(frame)
    {
        m_window.installEventFilter(this);
        if (m_frame != nullptr) {
            m_frame->installEventFilter(this);
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Resize) {
            if (watched == &m_window || watched == m_frame) {
                m_chrome.sync(windowIsMaximized(m_window));
            }
        }
        if (event->type() == QEvent::WindowStateChange && watched == &m_window) {
            m_chrome.onWindowStateChanged();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    FramelessShellChrome &m_chrome;
    QMainWindow &m_window;
    QWidget *m_frame = nullptr;
};

QRect gripGeometry(Qt::Edges edges, const QRect &area, int grip)
{
    const int w = area.width();
    const int h = area.height();
    if (edges == (Qt::TopEdge | Qt::LeftEdge)) {
        return QRect(0, 0, grip, grip);
    }
    if (edges == Qt::TopEdge) {
        return QRect(grip, 0, w - 2 * grip, grip);
    }
    if (edges == (Qt::TopEdge | Qt::RightEdge)) {
        return QRect(w - grip, 0, grip, grip);
    }
    if (edges == Qt::LeftEdge) {
        return QRect(0, grip, grip, h - 2 * grip);
    }
    if (edges == Qt::RightEdge) {
        return QRect(w - grip, grip, grip, h - 2 * grip);
    }
    if (edges == (Qt::BottomEdge | Qt::LeftEdge)) {
        return QRect(0, h - grip, grip, grip);
    }
    if (edges == Qt::BottomEdge) {
        return QRect(grip, h - grip, w - 2 * grip, grip);
    }
    if (edges == (Qt::BottomEdge | Qt::RightEdge)) {
        return QRect(w - grip, h - grip, grip, grip);
    }
    return {};
}

} // namespace

FramelessShellChrome::FramelessShellChrome(QMainWindow &window)
    : m_window(window)
{
}

void FramelessShellChrome::setFrame(QWidget *frame)
{
    m_frame = frame;
    if (m_frame != nullptr) {
        new ChromeSyncFilter(*this, m_window, m_frame);
    }
}

void FramelessShellChrome::setChromeRadius(int radiusPx)
{
    m_radiusPx = qMax(0, radiusPx);
}

void FramelessShellChrome::setLayoutChangeCallback(std::function<void()> callback)
{
    m_layoutChangeCallback = std::move(callback);
}

void FramelessShellChrome::onWindowStateChanged()
{
    runLayoutChangeCallback(m_layoutChangeCallback);
    sync(windowIsMaximized(m_window));
}

void FramelessShellChrome::sync(bool maximized)
{
    ensureGrips();
    layoutGrips();
    for (QWidget *grip : m_grips) {
        grip->setVisible(!maximized);
        grip->raise();
    }
    applyFrameMask(maximized);
}

void FramelessShellChrome::ensureGrips()
{
    if (m_gripsBuilt) {
        return;
    }
    m_gripsBuilt = true;

    const Qt::Edges gripEdges[] = {
        Qt::TopEdge | Qt::LeftEdge,
        Qt::TopEdge,
        Qt::TopEdge | Qt::RightEdge,
        Qt::LeftEdge,
        Qt::RightEdge,
        Qt::BottomEdge | Qt::LeftEdge,
        Qt::BottomEdge,
        Qt::BottomEdge | Qt::RightEdge,
    };
    for (Qt::Edges edges : gripEdges) {
        auto *grip = new ResizeGrip(edges, &m_window);
        grip->setObjectName(QStringLiteral("AvarResizeGrip"));
        m_grips.push_back(grip);
    }
}

void FramelessShellChrome::layoutGrips()
{
    const QRect area = m_window.rect();
    for (QWidget *grip : m_grips) {
        const Qt::Edges edges = static_cast<Qt::Edges>(grip->property(kResizeEdgesProperty).toInt());
        grip->setGeometry(gripGeometry(edges, area, kResizeGrip));
    }
}

void FramelessShellChrome::applyFrameMask(bool maximized)
{
    if (m_frame == nullptr) {
        return;
    }
    if (maximized || m_radiusPx <= 0) {
        m_frame->clearMask();
        return;
    }
    applyRoundedMask(m_frame, m_radiusPx);
}

} // namespace avar::gui
