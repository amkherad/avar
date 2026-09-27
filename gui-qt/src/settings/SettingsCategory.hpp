#pragma once

#include <QString>

namespace avar::gui {

enum class SettingsCategory {
    General,
    Downloads,
    Queues,
    Daemon,
    Browser,
    Shortcuts,
    About,
};

inline QString settingsCategoryKey(SettingsCategory category)
{
    switch (category) {
    case SettingsCategory::General:
        return QStringLiteral("general");
    case SettingsCategory::Downloads:
        return QStringLiteral("downloads");
    case SettingsCategory::Queues:
        return QStringLiteral("queues");
    case SettingsCategory::Daemon:
        return QStringLiteral("daemon");
    case SettingsCategory::Browser:
        return QStringLiteral("browser");
    case SettingsCategory::Shortcuts:
        return QStringLiteral("shortcuts");
    case SettingsCategory::About:
        return QStringLiteral("about");
    }
    return QStringLiteral("general");
}

} // namespace avar::gui
