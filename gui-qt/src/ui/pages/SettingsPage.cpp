#include "ui/pages/SettingsPage.hpp"

#include "ui/settings/SettingsContext.hpp"
#include "ui/settings/SettingsPanels.hpp"

#include <QStackedWidget>
#include <QVBoxLayout>

namespace avar::gui {

SettingsPage::SettingsPage(const SettingsContext &context, QWidget *parent)
    : QWidget(parent)
    , m_context(context)
{
    m_stack = new QStackedWidget(this);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_stack);

    populateSettingsStack(m_stack, m_context, this);
    setCategory(SettingsCategory::General);
}

void SettingsPage::setCategory(SettingsCategory category)
{
    showSettingsCategory(m_stack, category);
}

} // namespace avar::gui
