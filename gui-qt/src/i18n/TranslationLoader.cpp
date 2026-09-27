#include "i18n/TranslationLoader.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace avar::gui {

namespace {

void flattenObject(const QJsonObject &object, const QString &prefix, QHash<QString, QString> &out)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QString key = prefix.isEmpty() ? it.key() : QStringLiteral("%1.%2").arg(prefix, it.key());
        if (it.value().isObject()) {
            flattenObject(it.value().toObject(), key, out);
        } else if (it.value().isString()) {
            out.insert(key, it.value().toString());
        }
    }
}

QHash<QString, QString> loadJsonResource(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }
    QHash<QString, QString> flat;
    flattenObject(doc.object(), {}, flat);
    return flat;
}

void mergeInto(QHash<QString, QString> &base, const QHash<QString, QString> &overlay)
{
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        base.insert(it.key(), it.value());
    }
}

void applyLegacyAliases(QHash<QString, QString> &strings)
{
    static const QHash<QString, QString> aliases = {
        {QStringLiteral("theme.lightSoft"), QStringLiteral("settings.themeLight")},
        {QStringLiteral("theme.lightBright"), QStringLiteral("settings.themeLightBright")},
        {QStringLiteral("theme.queenMode"), QStringLiteral("settings.themeQueenMode")},
        {QStringLiteral("theme.dark"), QStringLiteral("settings.themeDark")},
        {QStringLiteral("theme.system"), QStringLiteral("settings.themeSystem")},
        {QStringLiteral("theme.toggle"), QStringLiteral("nav.themeToggle")},
        {QStringLiteral("settings.general"), QStringLiteral("settings.categories.general")},
    };
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        if (!strings.contains(it.key())) {
            const QString mapped = strings.value(it.value());
            if (!mapped.isEmpty()) {
                strings.insert(it.key(), mapped);
            }
        }
    }
}

} // namespace

QHash<QString, QString> TranslationLoader::loadMerged(const QString &localeId)
{
    QHash<QString, QString> strings = loadJsonResource(QStringLiteral(":/i18n/en.json"));
    if (localeId != QStringLiteral("en")) {
        mergeInto(strings, loadJsonResource(QStringLiteral(":/i18n/%1.json").arg(localeId)));
    }
    mergeInto(strings, loadJsonResource(QStringLiteral(":/i18n/supplement-en.json")));
    if (localeId != QStringLiteral("en")) {
        mergeInto(strings, loadJsonResource(QStringLiteral(":/i18n/supplement-%1.json").arg(localeId)));
    }
    applyLegacyAliases(strings);
    return strings;
}

} // namespace avar::gui
