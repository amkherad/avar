#include "theme/StylesheetBuilder.hpp"

#include <QApplication>
#include <QByteArray>

namespace avar::gui {

namespace {

QString rtlControlOverrides(const ThemeTokens &tokens)
{
    const int innerRadius = qMax(4, tokens.radiusPx - 2);
    return QStringLiteral(
        "QComboBox { padding-left: 24px; padding-right: 0px; }"
        "QComboBox::drop-down {"
        " subcontrol-position: top left;"
        " border-left: none;"
        " border-right: 1px solid %1;"
        " border-top-left-radius: %2px;"
        " border-bottom-left-radius: %2px;"
        " border-top-right-radius: 0;"
        " border-bottom-right-radius: 0;"
        "}"
        "QComboBox::down-arrow { margin-left: 5px; margin-right: 0px; }"
        "QSpinBox::up-button, QSpinBox::down-button,"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        " border-left: none;"
        " border-right: 1px solid %1;"
        "}"
    )
        .arg(tokens.border, QString::number(innerRadius));
}

QString dropdownChevronDataUri(const QString &color)
{
    const QString svg = QStringLiteral(
                            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
                            "<path fill='%1' d='M4.2 5.7 8 9.5l3.8-3.8.9.9L8 11.3 3.3 6.6l.9-.9z'/>"
                            "</svg>")
                            .arg(color);
    return QStringLiteral("data:image/svg+xml;base64,")
           + QString::fromLatin1(svg.toUtf8().toBase64());
}

} // namespace

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
    replace(QStringLiteral("@@BORDER_SUBTLE@@"), tokens.borderSubtle);
    replace(QStringLiteral("@@BORDER_CHROME@@"), tokens.borderChrome);
    replace(QStringLiteral("@@TEXT@@"), tokens.text);
    replace(QStringLiteral("@@TEXT_MUTED@@"), tokens.textMuted);
    replace(QStringLiteral("@@PRIMARY@@"), tokens.primary);
    replace(QStringLiteral("@@PRIMARY_HOVER@@"), tokens.primaryHover);
    replace(QStringLiteral("@@PRIMARY_TEXT@@"), tokens.primaryText);
    replace(QStringLiteral("@@SUCCESS@@"), tokens.success);
    replace(QStringLiteral("@@WARNING@@"), tokens.warning);
    replace(QStringLiteral("@@DANGER@@"), tokens.danger);
    replace(QStringLiteral("@@RADIUS@@"), QString::number(tokens.radiusPx));
    const int innerRadius = qMax(4, tokens.radiusPx - 2);
    replace(QStringLiteral("@@RADIUS_INNER@@"), QString::number(innerRadius));
    replace(QStringLiteral("@@FONT@@"), tokens.fontFamily);
    replace(QStringLiteral("@@DROPDOWN_CHEVRON@@"), dropdownChevronDataUri(tokens.textMuted));
    if (QApplication::layoutDirection() == Qt::RightToLeft) {
        sheet += rtlControlOverrides(tokens);
    }
    return sheet;
}

} // namespace avar::gui
