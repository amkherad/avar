#include "ui/pages/HelpPage.hpp"

#include "i18n/Translator.hpp"

#include <QLabel>
#include <QVBoxLayout>

namespace avar::gui {

HelpPage::HelpPage(Translator &translator, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *label = new QLabel(translator.tr(QStringLiteral("help.welcome")), this);
    label->setWordWrap(true);
    layout->addWidget(label);
    layout->addStretch();
}

} // namespace avar::gui
