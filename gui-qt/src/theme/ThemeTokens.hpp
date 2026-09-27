#pragma once

#include <QString>

namespace avar::gui {

enum class ThemeSetting {
    LightSoft,
    LightBright,
    QueenMode,
    Dark,
    System,
};

struct ThemeTokens {
    QString id;
    QString bg;
    QString bgElevated;
    QString bgMuted;
    QString border;
    /** Lower-contrast border on elevated surfaces (e.g. cards). */
    QString borderSubtle;
    /** Lower-contrast window chrome on main background. */
    QString borderChrome;
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
ThemeTokens queenModeTheme();
ThemeTokens darkTheme();

ThemeTokens resolveThemeTokens(ThemeSetting setting, bool systemPrefersDark);

} // namespace avar::gui
