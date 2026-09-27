#include "ui/widgets/AvarButton.hpp"

namespace avar::gui {

AvarButton::AvarButton(AvarButtonVariant variant, QWidget *parent)
    : QPushButton(parent)
{
    QString className;
    switch (variant) {
    case AvarButtonVariant::Primary:
        className = QStringLiteral("AvarButtonPrimary");
        break;
    case AvarButtonVariant::Secondary:
        className = QStringLiteral("AvarButtonSecondary");
        break;
    case AvarButtonVariant::Ghost:
        className = QStringLiteral("AvarButtonGhost");
        break;
    case AvarButtonVariant::Danger:
        className = QStringLiteral("AvarButtonDanger");
        break;
    }
    setProperty("class", className);
}

} // namespace avar::gui
