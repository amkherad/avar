#include "theme/ThemeManager.hpp"

#include "config/AppSettings.hpp"
#include "theme/StylesheetBuilder.hpp"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace avar::gui {

namespace {

void ensureFusionStyle()
{
    if (qApp->style()->objectName() != QStringLiteral("Fusion")) {
        if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
            qApp->setStyle(fusion);
        }
    }
}

QString loadStylesheetTemplate()
{
    QFile templateFile(QStringLiteral(":/styles/app.qss"));
    if (!templateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(templateFile.readAll());
}

void installApplicationStylesheet(const ThemeTokens &tokens)
{
    qApp->setStyleSheet(buildApplicationStylesheet(tokens, loadStylesheetTemplate()));
}

} // namespace

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
    ensureFusionStyle();

    QPalette palette;
    const QColor window = QColor(m_tokens.bg);
    const QColor elevated = QColor(m_tokens.bgElevated);
    const QColor muted = QColor(m_tokens.bgMuted);
    const QColor border = QColor(m_tokens.border);
    const QColor text = QColor(m_tokens.text);
    const QColor textMuted = QColor(m_tokens.textMuted);
    const QColor primary = QColor(m_tokens.primary);
    const QColor primaryText = QColor(m_tokens.primaryText);

    const auto applyPaletteGroup = [&](QPalette::ColorGroup group) {
        if (group == QPalette::Disabled) {
            palette.setColor(group, QPalette::Window, window);
            palette.setColor(group, QPalette::WindowText, textMuted);
            palette.setColor(group, QPalette::Base, muted);
            palette.setColor(group, QPalette::AlternateBase, muted);
            palette.setColor(group, QPalette::Text, textMuted);
            palette.setColor(group, QPalette::Button, muted);
            palette.setColor(group, QPalette::ButtonText, textMuted);
            palette.setColor(group, QPalette::BrightText, textMuted);
            palette.setColor(group, QPalette::PlaceholderText, textMuted);
            palette.setColor(group, QPalette::Highlight, primary);
            palette.setColor(group, QPalette::HighlightedText, primaryText);
            palette.setColor(group, QPalette::Link, primary);
            palette.setColor(group, QPalette::Mid, border);
            palette.setColor(group, QPalette::Dark, border);
            palette.setColor(group, QPalette::Light, muted);
            palette.setColor(group, QPalette::Shadow, border);
            palette.setColor(group, QPalette::ToolTipBase, elevated);
            palette.setColor(group, QPalette::ToolTipText, text);
            return;
        }
        palette.setColor(group, QPalette::Window, window);
        palette.setColor(group, QPalette::WindowText, text);
        palette.setColor(group, QPalette::Base, elevated);
        palette.setColor(group, QPalette::AlternateBase, muted);
        palette.setColor(group, QPalette::Text, text);
        palette.setColor(group, QPalette::Button, muted);
        palette.setColor(group, QPalette::ButtonText, text);
        palette.setColor(group, QPalette::BrightText, text);
        palette.setColor(group, QPalette::PlaceholderText, textMuted);
        palette.setColor(group, QPalette::Highlight, primary);
        palette.setColor(group, QPalette::HighlightedText, primaryText);
        palette.setColor(group, QPalette::Link, primary);
        palette.setColor(group, QPalette::Mid, border);
        palette.setColor(group, QPalette::Dark, border);
        palette.setColor(group, QPalette::Light, muted);
        palette.setColor(group, QPalette::Shadow, border);
        palette.setColor(group, QPalette::ToolTipBase, elevated);
        palette.setColor(group, QPalette::ToolTipText, text);
    };

    applyPaletteGroup(QPalette::Active);
    applyPaletteGroup(QPalette::Inactive);
    applyPaletteGroup(QPalette::Disabled);
    qApp->setPalette(palette);

    installApplicationStylesheet(m_tokens);

    emit themeChanged(m_tokens);
}

void ThemeManager::syncStylesheet()
{
    installApplicationStylesheet(m_tokens);
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
        m_settings.setThemeSetting(ThemeSetting::QueenMode);
        break;
    case ThemeSetting::QueenMode:
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
