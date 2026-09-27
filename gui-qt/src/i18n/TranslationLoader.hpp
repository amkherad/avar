#pragma once

#include <QHash>
#include <QString>

namespace avar::gui {

class TranslationLoader {
public:
    [[nodiscard]] static QHash<QString, QString> loadMerged(const QString &localeId);
};

} // namespace avar::gui
