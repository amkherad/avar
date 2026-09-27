#include "i18n/Translator.hpp"

#include "i18n/LocaleCatalog.hpp"
#include "i18n/TranslationLoader.hpp"

namespace avar::gui {

Translator::Translator(QObject *parent)
    : QObject(parent)
{
    reloadStrings();
}

void Translator::setLocale(const QString &locale)
{
    const QString normalized = LocaleCatalog::normalizeLocaleId(locale);
    if (m_locale == normalized) {
        return;
    }
    m_locale = normalized;
    reloadStrings();
    emit translationsChanged();
}

QString Translator::locale() const
{
    return m_locale;
}

bool Translator::isRtl() const
{
    return LocaleCatalog::isRtlLocale(m_locale);
}

void Translator::reloadStrings()
{
    m_strings = TranslationLoader::loadMerged(m_locale);
}

QString Translator::tr(const QString &key) const
{
    const QString value = m_strings.value(key);
    if (!value.isEmpty()) {
        return value;
    }
    return key;
}

} // namespace avar::gui
