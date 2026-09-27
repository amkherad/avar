#include "i18n/LocaleCatalog.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace avar::gui {

namespace {

QVector<LocaleDescriptor> loadCatalog()
{
    QFile file(QStringLiteral(":/i18n/locales.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {{QStringLiteral("en"), QStringLiteral("English"), false},
                {QStringLiteral("fa"), QStringLiteral("فارسی"), true}};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return {{QStringLiteral("en"), QStringLiteral("English"), false}};
    }

    QVector<LocaleDescriptor> entries;
    for (const QJsonValue &value : doc.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        LocaleDescriptor entry;
        entry.id = obj.value(QStringLiteral("id")).toString();
        entry.nameNative = obj.value(QStringLiteral("nameNative")).toString();
        entry.rtl = obj.value(QStringLiteral("rtl")).toBool(false);
        if (!entry.id.isEmpty()) {
            entries.push_back(entry);
        }
    }
    return entries;
}

} // namespace

QVector<LocaleDescriptor> LocaleCatalog::availableLocales()
{
    static const QVector<LocaleDescriptor> catalog = loadCatalog();
    return catalog;
}

bool LocaleCatalog::isRtlLocale(const QString &localeId)
{
    const QString id = normalizeLocaleId(localeId);
    for (const LocaleDescriptor &entry : availableLocales()) {
        if (entry.id == id) {
            return entry.rtl;
        }
    }
    return id == QStringLiteral("fa");
}

QString LocaleCatalog::normalizeLocaleId(const QString &localeId)
{
    const QString trimmed = localeId.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("en");
    }
    for (const LocaleDescriptor &entry : availableLocales()) {
        if (entry.id == trimmed) {
            return entry.id;
        }
    }
    return QStringLiteral("en");
}

} // namespace avar::gui
