#pragma once

#include <QString>
#include <QVector>

namespace avar::gui {

struct HelpTopic {
    QString id;
    QString titleKey;
    QString file;
};

class HelpDocs {
public:
    [[nodiscard]] static QVector<HelpTopic> topics();
    [[nodiscard]] static QString defaultTopicId();
    [[nodiscard]] static HelpTopic topicById(const QString &id);
    [[nodiscard]] static QString loadContent(const QString &locale, const QString &file);
};

} // namespace avar::gui
