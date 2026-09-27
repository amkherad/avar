#pragma once

#include <QWidget>

namespace avar::gui {

enum class ResizeAxis {
    Horizontal,
    Vertical,
};

class ResizeHandle final : public QWidget {
    Q_OBJECT

public:
    ResizeHandle(ResizeAxis axis, QWidget *parent = nullptr);

signals:
    void resizeDelta(int delta);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    ResizeAxis m_axis;
    QPoint m_pressPos;
};

} // namespace avar::gui
