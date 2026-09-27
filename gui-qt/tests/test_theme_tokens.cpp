#include "theme/StylesheetBuilder.hpp"
#include "theme/ThemeTokens.hpp"

#include <QtTest>

using namespace avar::gui;

class ThemeTokensTest final : public QObject {
    Q_OBJECT

private slots:
    void darkThemeMatchesElectronPalette();
    void stylesheetReplacesTokens();
};

void ThemeTokensTest::darkThemeMatchesElectronPalette()
{
    const ThemeTokens tokens = darkTheme();
    QCOMPARE(tokens.bg, QStringLiteral("#0f1419"));
    QCOMPARE(tokens.primary, QStringLiteral("#3b82f6"));
}

void ThemeTokensTest::stylesheetReplacesTokens()
{
    const ThemeTokens tokens = softLightTheme();
    const QString sheet = buildApplicationStylesheet(tokens, QStringLiteral("bg: @@BG@@;"));
    QCOMPARE(sheet, QStringLiteral("bg: #d4d0c8;"));
}

QTEST_MAIN(ThemeTokensTest)
#include "test_theme_tokens.moc"
