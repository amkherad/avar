#include "theme/StylesheetBuilder.hpp"

namespace avar::gui {

QString buildApplicationStylesheet(const ThemeTokens &tokens, const QString &baseTemplate)
{
    QString sheet = baseTemplate;
    const auto replace = [&](const QString &key, const QString &value) {
        sheet.replace(key, value);
    };

    replace(QStringLiteral("@@BG@@"), tokens.bg);
    replace(QStringLiteral("@@BG_ELEVATED@@"), tokens.bgElevated);
    replace(QStringLiteral("@@BG_MUTED@@"), tokens.bgMuted);
    replace(QStringLiteral("@@BORDER@@"), tokens.border);
    replace(QStringLiteral("@@TEXT@@"), tokens.text);
    replace(QStringLiteral("@@TEXT_MUTED@@"), tokens.textMuted);
    replace(QStringLiteral("@@PRIMARY@@"), tokens.primary);
    replace(QStringLiteral("@@PRIMARY_HOVER@@"), tokens.primaryHover);
    replace(QStringLiteral("@@PRIMARY_TEXT@@"), tokens.primaryText);
    replace(QStringLiteral("@@SUCCESS@@"), tokens.success);
    replace(QStringLiteral("@@WARNING@@"), tokens.warning);
    replace(QStringLiteral("@@DANGER@@"), tokens.danger);
    replace(QStringLiteral("@@RADIUS@@"), QString::number(tokens.radiusPx));
    replace(QStringLiteral("@@FONT@@"), tokens.fontFamily);
    return sheet;
}

} // namespace avar::gui
