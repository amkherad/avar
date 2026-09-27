#include "theme/ThemeTokens.hpp"

namespace avar::gui {

namespace {

ThemeTokens makeLightSoft()
{
    ThemeTokens t;
    t.id = QStringLiteral("light-soft");
    t.bg = QStringLiteral("#d4d0c8");
    t.bgElevated = QStringLiteral("#e2ded6");
    t.bgMuted = QStringLiteral("#c8c4bc");
    t.border = QStringLiteral("#b4b0a8");
    t.text = QStringLiteral("#2a2824");
    t.textMuted = QStringLiteral("#5c5850");
    t.primary = QStringLiteral("#2563eb");
    t.primaryHover = QStringLiteral("#1d4ed8");
    t.primaryText = QStringLiteral("#f4f2ee");
    t.success = QStringLiteral("#15803d");
    t.warning = QStringLiteral("#b45309");
    t.danger = QStringLiteral("#b91c1c");
    t.shadowDescription = QStringLiteral("0 8px 24px rgba(42, 40, 36, 0.12)");
    t.radiusPx = 10;
    t.fontFamily = QStringLiteral("Segoe UI");
    return t;
}

ThemeTokens makeLightBright()
{
    ThemeTokens t;
    t.id = QStringLiteral("light-bright");
    t.bg = QStringLiteral("#f4f6f8");
    t.bgElevated = QStringLiteral("#ffffff");
    t.bgMuted = QStringLiteral("#e8ecf0");
    t.border = QStringLiteral("#d5dbe3");
    t.text = QStringLiteral("#1a2332");
    t.textMuted = QStringLiteral("#5c6b7f");
    t.primary = QStringLiteral("#2563eb");
    t.primaryHover = QStringLiteral("#1d4ed8");
    t.primaryText = QStringLiteral("#ffffff");
    t.success = QStringLiteral("#16a34a");
    t.warning = QStringLiteral("#d97706");
    t.danger = QStringLiteral("#dc2626");
    t.shadowDescription = QStringLiteral("0 8px 24px rgba(15, 23, 42, 0.08)");
    t.radiusPx = 10;
    t.fontFamily = QStringLiteral("Segoe UI");
    return t;
}

ThemeTokens makeDark()
{
    ThemeTokens t;
    t.id = QStringLiteral("dark");
    t.bg = QStringLiteral("#0f1419");
    t.bgElevated = QStringLiteral("#1a222d");
    t.bgMuted = QStringLiteral("#242d3a");
    t.border = QStringLiteral("#334155");
    t.text = QStringLiteral("#e8edf4");
    t.textMuted = QStringLiteral("#94a3b8");
    t.primary = QStringLiteral("#3b82f6");
    t.primaryHover = QStringLiteral("#60a5fa");
    t.primaryText = QStringLiteral("#0f1419");
    t.success = QStringLiteral("#22c55e");
    t.warning = QStringLiteral("#f59e0b");
    t.danger = QStringLiteral("#ef4444");
    t.shadowDescription = QStringLiteral("0 8px 24px rgba(0, 0, 0, 0.35)");
    t.radiusPx = 10;
    t.fontFamily = QStringLiteral("Segoe UI");
    return t;
}

} // namespace

ThemeTokens softLightTheme()
{
    return makeLightSoft();
}

ThemeTokens brightLightTheme()
{
    return makeLightBright();
}

ThemeTokens darkTheme()
{
    return makeDark();
}

ThemeTokens resolveThemeTokens(ThemeSetting setting, bool systemPrefersDark)
{
    switch (setting) {
    case ThemeSetting::Dark:
        return makeDark();
    case ThemeSetting::LightBright:
        return makeLightBright();
    case ThemeSetting::System:
        return systemPrefersDark ? makeDark() : makeLightSoft();
    case ThemeSetting::LightSoft:
    default:
        return makeLightSoft();
    }
}

} // namespace avar::gui
