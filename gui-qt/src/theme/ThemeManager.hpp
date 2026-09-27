#pragma once

#include "theme/ThemeTokens.hpp"

#include <QObject>

namespace avar::gui {

class AppSettings;

class ThemeManager final : public QObject {
    Q_OBJECT

public:
    explicit ThemeManager(AppSettings &settings, QObject *parent = nullptr);

    void apply();
    [[nodiscard]] ThemeTokens currentTokens() const;

    void setSetting(ThemeSetting setting);
    void cycleTheme();
    void toggleResolvedTheme();

signals:
    void themeChanged(const ThemeTokens &tokens);

private:
    void refreshFromSettings();

    AppSettings &m_settings;
    ThemeTokens m_tokens;
};

} // namespace avar::gui
