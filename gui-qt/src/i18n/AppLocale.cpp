#include "i18n/AppLocale.hpp"

#include "i18n/Translator.hpp"

#include <QApplication>
#include <QWidget>

namespace avar::gui {

void applyApplicationLocale(QApplication &app, const Translator &translator)
{
    const auto direction = translator.isRtl() ? Qt::RightToLeft : Qt::LeftToRight;
    app.setLayoutDirection(direction);
    for (QWidget *widget : app.topLevelWidgets()) {
        widget->setLayoutDirection(direction);
    }
}

} // namespace avar::gui
