#pragma once

#include <QString>
#include <QVector>

namespace avar::gui {

struct LocaleDescriptor {
    QString id;
    QString nameNative;
    bool rtl = false;
};

class LocaleCatalog {
public:
    [[nodiscard]] static QVector<LocaleDescriptor> availableLocales();
    [[nodiscard]] static bool isRtlLocale(const QString &localeId);
    [[nodiscard]] static QString normalizeLocaleId(const QString &localeId);
};

} // namespace avar::gui
