#include "help/HelpDocs.hpp"

#include <QFile>

namespace avar::gui {

QVector<HelpTopic> HelpDocs::topics()
{
    return {
        {QStringLiteral("index"), QStringLiteral("help.topics.index"), QStringLiteral("index.md")},
        {QStringLiteral("downloads"), QStringLiteral("help.topics.downloads"), QStringLiteral("downloads.md")},
        {QStringLiteral("queues"), QStringLiteral("help.topics.queues"), QStringLiteral("queues.md")},
        {QStringLiteral("bookmarks"), QStringLiteral("help.topics.bookmarks"), QStringLiteral("bookmarks.md")},
        {QStringLiteral("sessions"), QStringLiteral("help.topics.sessions"), QStringLiteral("sessions.md")},
        {QStringLiteral("settings"), QStringLiteral("help.topics.settings"), QStringLiteral("settings.md")},
        {QStringLiteral("shortcuts"), QStringLiteral("help.topics.shortcuts"), QStringLiteral("shortcuts.md")},
        {QStringLiteral("console"), QStringLiteral("help.topics.console"), QStringLiteral("console.md")},
        {QStringLiteral("layout"), QStringLiteral("help.topics.layout"), QStringLiteral("layout.md")},
    };
}

QString HelpDocs::defaultTopicId()
{
    return QStringLiteral("index");
}

HelpTopic HelpDocs::topicById(const QString &id)
{
    for (const HelpTopic &topic : topics()) {
        if (topic.id == id) {
            return topic;
        }
    }
    return topics().front();
}

QString HelpDocs::loadContent(const QString &locale, const QString &file)
{
    const auto tryLocale = [](const QString &loc, const QString &markdownFile) -> QString {
        QFile resource(QStringLiteral(":/help/%1/%2").arg(loc, markdownFile));
        if (!resource.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        return QString::fromUtf8(resource.readAll());
    };

    QString content = tryLocale(locale, file);
    if (!content.isEmpty()) {
        return content;
    }
    return tryLocale(QStringLiteral("en"), file);
}

} // namespace avar::gui
