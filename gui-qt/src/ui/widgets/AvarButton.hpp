#pragma once

#include <QPushButton>

namespace avar::gui {

enum class AvarButtonVariant {
    Primary,
    Secondary,
    Ghost,
};

class AvarButton final : public QPushButton {
    Q_OBJECT

public:
    explicit AvarButton(AvarButtonVariant variant = AvarButtonVariant::Secondary,
                        QWidget *parent = nullptr);
};

} // namespace avar::gui
