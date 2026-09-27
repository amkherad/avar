#pragma once

#include <QString>

namespace avar::gui {

enum class ThemeSetting {
    LightSoft,
    LightBright,
    Dark,
    System,
};

struct ThemeTokens {
    QString id;
    QString bg;
    QString bgElevated;
    QString bgMuted;
    QString border;
    QString text;
    QString textMuted;
    QString primary;
    QString primaryHover;
    QString primaryText;
    QString success;
    QString warning;
    QString danger;
    QString shadowDescription;
    int radiusPx = 10;
    QString fontFamily;
};

ThemeTokens softLightTheme();
ThemeTokens brightLightTheme();
ThemeTokens darkTheme();

ThemeTokens resolveThemeTokens(ThemeSetting setting, bool systemPrefersDark);

} // namespace avar::gui
