#include "ui/widgets/HelpSidebarNav.hpp"

#include "help/HelpDocs.hpp"
#include "i18n/Translator.hpp"

#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

namespace avar::gui {

HelpSidebarNav::HelpSidebarNav(Translator &translator, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_topicId(HelpDocs::defaultTopicId())
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignTop);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *title = new QLabel(m_tr.tr(QStringLiteral("help.title")), this);
    title->setProperty("class", QStringLiteral("AvarPageTitle"));
    title->setObjectName(QStringLiteral("AvarHelpNavTitle"));
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(title);

    for (const HelpTopic &topic : HelpDocs::topics()) {
        auto *button = new QPushButton(m_tr.tr(topic.titleKey), this);
        button->setCheckable(true);
        button->setProperty("class", QStringLiteral("AvarSidebarNavItem"));
        button->setProperty("stripedIndex", m_buttons.size());
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        connect(button, &QPushButton::clicked, this, [this, id = topic.id] {
            setTopicId(id);
            emit topicChanged(id);
        });
        m_buttons.push_back(button);
        layout->addWidget(button);
    }

    layout->addStretch(1);
    setTopicId(m_topicId);
}

void HelpSidebarNav::retranslateUi()
{
    if (auto *title = findChild<QLabel *>(QStringLiteral("AvarHelpNavTitle"))) {
        title->setText(m_tr.tr(QStringLiteral("help.title")));
    }
    const QVector<HelpTopic> topics = HelpDocs::topics();
    for (int i = 0; i < m_buttons.size() && i < topics.size(); ++i) {
        m_buttons.at(i)->setText(m_tr.tr(topics.at(i).titleKey));
    }
}

void HelpSidebarNav::setTopicId(const QString &id)
{
    m_topicId = HelpDocs::topicById(id).id;
    for (int i = 0; i < m_buttons.size(); ++i) {
        QPushButton *button = m_buttons.at(i);
        const HelpTopic topic = HelpDocs::topics().at(i);
        const bool active = topic.id == m_topicId;
        button->setChecked(active);
        button->setProperty("active", active);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

} // namespace avar::gui
