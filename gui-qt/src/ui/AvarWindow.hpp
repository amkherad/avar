#pragma once

#include <QMainWindow>

class QWidget;

namespace avar::gui {

/** Secondary window with the same chrome and theme as the main shell (desktop). */
class AvarWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit AvarWindow(QWidget *parent = nullptr);

    void setContentWidget(QWidget *widget);
};

} // namespace avar::gui
