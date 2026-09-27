#include "ui/pages/HelpPage.hpp"

#include "config/AppSettings.hpp"
#include "help/HelpDocs.hpp"
#include "i18n/Translator.hpp"

#include <QFrame>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace avar::gui {

HelpPage::HelpPage(Translator &translator, AppSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_settings(settings)
    , m_topicId(HelpDocs::defaultTopicId())
{
    setObjectName(QStringLiteral("AvarHelpPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 24, 20);
    layout->setSpacing(0);

    m_browser = new QTextBrowser(this);
    m_browser->setObjectName(QStringLiteral("AvarHelpMarkdown"));
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setOpenExternalLinks(true);
    m_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_browser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_browser, 1);

    connect(&m_settings, &AppSettings::localeChanged, this, &HelpPage::reloadContent);
    connect(&m_tr, &Translator::translationsChanged, this, &HelpPage::reloadContent);
    reloadContent();
}

void HelpPage::setTopicId(const QString &id)
{
    m_topicId = HelpDocs::topicById(id).id;
    applyContent();
}

QString HelpPage::topicId() const
{
    return m_topicId;
}

void HelpPage::reloadContent()
{
    applyContent();
}

void HelpPage::applyContent()
{
    const HelpTopic topic = HelpDocs::topicById(m_topicId);
    const QString markdown = HelpDocs::loadContent(m_settings.locale(), topic.file);
    if (markdown.isEmpty()) {
        m_browser->setMarkdown(QStringLiteral("*%1*").arg(m_tr.tr(QStringLiteral("help.notFound"))));
        return;
    }
    m_browser->setMarkdown(markdown);
    if (QScrollBar *bar = m_browser->verticalScrollBar()) {
        bar->setValue(0);
    }
}

} // namespace avar::gui
