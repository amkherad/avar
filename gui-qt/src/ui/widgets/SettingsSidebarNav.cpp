#include "ui/widgets/SettingsSidebarNav.hpp"

#include "i18n/Translator.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace avar::gui {

SettingsSidebarNav::SettingsSidebarNav(Translator &translator, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignTop);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *title = new QLabel(m_tr.tr(QStringLiteral("settings.title")), this);
    title->setProperty("class", QStringLiteral("AvarPageTitle"));
    title->setObjectName(QStringLiteral("AvarSettingsNavTitle"));
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(title);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    const SettingsCategory categories[] = {
        SettingsCategory::General,
        SettingsCategory::Downloads,
        SettingsCategory::Queues,
        SettingsCategory::Daemon,
        SettingsCategory::Browser,
        SettingsCategory::Shortcuts,
        SettingsCategory::About,
    };

    for (SettingsCategory cat : categories) {
        const QString key = settingsCategoryKey(cat);
        auto *button = new QPushButton(m_tr.tr(QStringLiteral("settings.categories.") + key), this);
        button->setCheckable(true);
        button->setProperty("class", QStringLiteral("AvarSidebarNavItem"));
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_group->addButton(button, static_cast<int>(cat));
        layout->addWidget(button);
    }

    layout->addStretch(1);

    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        emit categoryChanged(static_cast<SettingsCategory>(id));
    });

    setCategory(SettingsCategory::General);
}

void SettingsSidebarNav::retranslateUi()
{
    if (auto *title = findChild<QLabel *>(QStringLiteral("AvarSettingsNavTitle"))) {
        title->setText(m_tr.tr(QStringLiteral("settings.title")));
    }
    const SettingsCategory categories[] = {
        SettingsCategory::General,
        SettingsCategory::Downloads,
        SettingsCategory::Queues,
        SettingsCategory::Daemon,
        SettingsCategory::Browser,
        SettingsCategory::Shortcuts,
        SettingsCategory::About,
    };
    for (QAbstractButton *button : m_group->buttons()) {
        const int id = m_group->id(button);
        if (id < 0 || id >= static_cast<int>(sizeof(categories) / sizeof(categories[0]))) {
            continue;
        }
        const QString key = settingsCategoryKey(static_cast<SettingsCategory>(id));
        button->setText(m_tr.tr(QStringLiteral("settings.categories.") + key));
    }
}

void SettingsSidebarNav::setCategory(SettingsCategory category)
{
    if (auto *button = m_group->button(static_cast<int>(category))) {
        button->setChecked(true);
        button->setProperty("active", true);
        for (auto *other : m_group->buttons()) {
            if (other != button) {
                other->setProperty("active", false);
            }
        }
    }
}

} // namespace avar::gui
