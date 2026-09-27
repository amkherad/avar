#pragma once

#include <QObject>

class QWidget;

namespace avar::gui {

/** Enables dragging the top-level window from a header (frameless desktop shell). */
class HeaderWindowDrag final : public QObject {
    Q_OBJECT

public:
    explicit HeaderWindowDrag(QWidget *header, QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_header = nullptr;
};

} // namespace avar::gui
