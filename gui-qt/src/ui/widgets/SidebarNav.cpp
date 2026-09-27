#include "ui/widgets/SidebarNav.hpp"

#include "i18n/Translator.hpp"

#include <QButtonGroup>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace avar::gui {

SidebarNav::SidebarNav(Translator &translator, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    const QStringList keys = {
        QStringLiteral("nav.dashboard"),
        QStringLiteral("nav.settings"),
        QStringLiteral("nav.help"),
    };

    for (int i = 0; i < keys.size(); ++i) {
        auto *button = new QPushButton(m_tr.tr(keys.at(i)), this);
        button->setCheckable(true);
        button->setProperty("class", QStringLiteral("AvarSidebarNavItem"));
        button->setProperty("active", false);
        m_group->addButton(button, i);
        layout->addWidget(button);
    }

    connect(m_group, &QButtonGroup::idClicked, this, &SidebarNav::itemActivated);

    setActiveIndex(0);
}

void SidebarNav::setActiveIndex(int index)
{
    if (auto *button = m_group->button(index)) {
        button->setChecked(true);
        button->setProperty("active", true);
        for (auto *other : m_group->buttons()) {
            if (other != button) {
                other->setProperty("active", false);
                other->style()->unpolish(other);
                other->style()->polish(other);
            }
        }
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

} // namespace avar::gui
