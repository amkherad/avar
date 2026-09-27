#include "theme/ThemeManager.hpp"

#include "config/AppSettings.hpp"
#include "theme/StylesheetBuilder.hpp"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

namespace avar::gui {

ThemeManager::ThemeManager(AppSettings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    connect(&m_settings, &AppSettings::themeSettingChanged, this, [this] { apply(); });
}

void ThemeManager::refreshFromSettings()
{
    bool systemDark = false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    systemDark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    systemDark = false;
#endif
    m_tokens = resolveThemeTokens(m_settings.themeSetting(), systemDark);
}

void ThemeManager::apply()
{
    refreshFromSettings();

    QFile templateFile(QStringLiteral(":/styles/app.qss"));
    QString base;
    if (templateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        base = QString::fromUtf8(templateFile.readAll());
    }

    qApp->setStyleSheet(buildApplicationStylesheet(m_tokens, base));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(m_tokens.bg));
    palette.setColor(QPalette::WindowText, QColor(m_tokens.text));
    palette.setColor(QPalette::Base, QColor(m_tokens.bgElevated));
    palette.setColor(QPalette::AlternateBase, QColor(m_tokens.bgMuted));
    palette.setColor(QPalette::Text, QColor(m_tokens.text));
    palette.setColor(QPalette::Button, QColor(m_tokens.bgMuted));
    palette.setColor(QPalette::ButtonText, QColor(m_tokens.text));
    palette.setColor(QPalette::Highlight, QColor(m_tokens.primary));
    palette.setColor(QPalette::HighlightedText, QColor(m_tokens.primaryText));
    qApp->setPalette(palette);

    emit themeChanged(m_tokens);
}

ThemeTokens ThemeManager::currentTokens() const
{
    return m_tokens;
}

void ThemeManager::setSetting(ThemeSetting setting)
{
    m_settings.setThemeSetting(setting);
}

void ThemeManager::toggleResolvedTheme()
{
    const bool dark = m_tokens.id == QStringLiteral("dark");
    m_settings.setThemeSetting(dark ? ThemeSetting::LightSoft : ThemeSetting::Dark);
}

void ThemeManager::cycleTheme()
{
    switch (m_settings.themeSetting()) {
    case ThemeSetting::LightSoft:
        m_settings.setThemeSetting(ThemeSetting::LightBright);
        break;
    case ThemeSetting::LightBright:
        m_settings.setThemeSetting(ThemeSetting::Dark);
        break;
    case ThemeSetting::Dark:
        m_settings.setThemeSetting(ThemeSetting::System);
        break;
    case ThemeSetting::System:
        m_settings.setThemeSetting(ThemeSetting::LightSoft);
        break;
    }
}

} // namespace avar::gui
