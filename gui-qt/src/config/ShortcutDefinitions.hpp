#pragma once

#include <QString>
#include <QVector>

namespace avar::gui {

struct ShortcutDefinition {
    QString id;
    QString defaultCombo;
    QString labelKey;
    QString categoryKey;
};

inline QVector<ShortcutDefinition> shortcutDefinitions()
{
    return {
        {QStringLiteral("download.add"), QStringLiteral("ctrl+n"),
         QStringLiteral("shortcuts.actions.downloadAdd"), QStringLiteral("shortcuts.category.downloads")},
        {QStringLiteral("download.search"), QStringLiteral("ctrl+f"),
         QStringLiteral("shortcuts.actions.downloadSearch"), QStringLiteral("shortcuts.category.downloads")},
        {QStringLiteral("download.pause"), QStringLiteral("ctrl+p"),
         QStringLiteral("shortcuts.actions.downloadPause"), QStringLiteral("shortcuts.category.downloads")},
        {QStringLiteral("download.start"), QStringLiteral("ctrl+shift+s"),
         QStringLiteral("shortcuts.actions.downloadStart"), QStringLiteral("shortcuts.category.downloads")},
        {QStringLiteral("download.stop"), QStringLiteral("ctrl+shift+x"),
         QStringLiteral("shortcuts.actions.downloadStop"), QStringLiteral("shortcuts.category.downloads")},
        {QStringLiteral("download.delete"), QStringLiteral("delete"),
         QStringLiteral("shortcuts.actions.downloadDelete"), QStringLiteral("shortcuts.category.downloads")},
        {QStringLiteral("nav.dashboard"), QStringLiteral("ctrl+1"),
         QStringLiteral("shortcuts.actions.navDashboard"), QStringLiteral("shortcuts.category.navigation")},
        {QStringLiteral("nav.settings"), QStringLiteral("ctrl+,"),
         QStringLiteral("shortcuts.actions.navSettings"), QStringLiteral("shortcuts.category.navigation")},
        {QStringLiteral("nav.help"), QStringLiteral("f1"), QStringLiteral("shortcuts.actions.navHelp"),
         QStringLiteral("shortcuts.category.navigation")},
        {QStringLiteral("console.toggle"), QStringLiteral("ctrl+`"),
         QStringLiteral("shortcuts.actions.consoleToggle"), QStringLiteral("shortcuts.category.view")},
        {QStringLiteral("detailPanel.toggle"), QStringLiteral("ctrl+d"),
         QStringLiteral("shortcuts.actions.detailPanelToggle"), QStringLiteral("shortcuts.category.view")},
    };
}

inline QString formatShortcutCombo(const QString &combo)
{
    QStringList parts;
    for (const QString &part : combo.split(QLatin1Char('+'))) {
        if (part == QStringLiteral("ctrl")) {
            parts.append(QStringLiteral("Ctrl"));
        } else if (part == QStringLiteral("shift")) {
            parts.append(QStringLiteral("Shift"));
        } else if (part == QStringLiteral("alt")) {
            parts.append(QStringLiteral("Alt"));
        } else if (part == QStringLiteral("meta")) {
            parts.append(QStringLiteral("Meta"));
        } else if (part == QStringLiteral("delete")) {
            parts.append(QStringLiteral("Del"));
        } else if (part == QStringLiteral("esc")) {
            parts.append(QStringLiteral("Esc"));
        } else if (part.size() == 1) {
            parts.append(part.toUpper());
        } else {
            parts.append(part);
        }
    }
    return parts.join(QStringLiteral(" + "));
}

} // namespace avar::gui
