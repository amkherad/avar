#include "config/LayoutPreferences.hpp"

#include <QSettings>

namespace avar::gui {

namespace {
constexpr int kSidebarMin = 180;
constexpr int kSidebarMax = 480;
constexpr int kConsoleMin = 120;
constexpr int kConsoleMax = 600;
constexpr int kDetailMin = 220;
constexpr int kDetailMax = 560;

int clamp(int value, int minVal, int maxVal)
{
    return qBound(minVal, value, maxVal);
}
} // namespace

LayoutPreferences::LayoutPreferences(QObject *parent)
    : QObject(parent)
{
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    m_sidebarWidth = store.value(QStringLiteral("layout/sidebarWidth"), 260).toInt();
    m_consoleHeight = store.value(QStringLiteral("layout/consoleHeight"), 220).toInt();
    m_detailPanelWidth = store.value(QStringLiteral("layout/detailPanelWidth"), 300).toInt();
    m_detailPanelOpen = store.value(QStringLiteral("layout/detailPanelOpen"), false).toBool();
    m_consoleOpen = store.value(QStringLiteral("layout/consoleOpen"), false).toBool();
    const QString view = store.value(QStringLiteral("layout/downloadViewMode"), QStringLiteral("grid")).toString();
    m_downloadViewMode = view == QStringLiteral("compact") ? DownloadViewMode::Compact : DownloadViewMode::Grid;
}

int LayoutPreferences::sidebarWidth() const
{
    return m_sidebarWidth;
}

int LayoutPreferences::consoleHeight() const
{
    return m_consoleHeight;
}

int LayoutPreferences::detailPanelWidth() const
{
    return m_detailPanelWidth;
}

bool LayoutPreferences::detailPanelOpen() const
{
    return m_detailPanelOpen;
}

bool LayoutPreferences::consoleOpen() const
{
    return m_consoleOpen;
}

DownloadViewMode LayoutPreferences::downloadViewMode() const
{
    return m_downloadViewMode;
}

void LayoutPreferences::setSidebarWidth(int width)
{
    m_sidebarWidth = clamp(width, kSidebarMin, kSidebarMax);
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("layout/sidebarWidth"),
                                                                         m_sidebarWidth);
    emit layoutChanged();
}

void LayoutPreferences::adjustSidebarWidth(int delta)
{
    setSidebarWidth(m_sidebarWidth + delta);
}

void LayoutPreferences::setConsoleHeight(int height)
{
    m_consoleHeight = clamp(height, kConsoleMin, kConsoleMax);
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("layout/consoleHeight"),
                                                                           m_consoleHeight);
    emit layoutChanged();
}

void LayoutPreferences::adjustConsoleHeight(int delta)
{
    setConsoleHeight(m_consoleHeight - delta);
}

void LayoutPreferences::setDetailPanelWidth(int width)
{
    m_detailPanelWidth = clamp(width, kDetailMin, kDetailMax);
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("layout/detailPanelWidth"),
                                                                          m_detailPanelWidth);
    emit layoutChanged();
}

void LayoutPreferences::adjustDetailPanelWidth(int delta)
{
    setDetailPanelWidth(m_detailPanelWidth + delta);
}

void LayoutPreferences::setDetailPanelOpen(bool open)
{
    m_detailPanelOpen = open;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("layout/detailPanelOpen"), open);
    emit layoutChanged();
}

void LayoutPreferences::setConsoleOpen(bool open)
{
    m_consoleOpen = open;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("layout/consoleOpen"), open);
    emit layoutChanged();
}

void LayoutPreferences::setDownloadViewMode(DownloadViewMode mode)
{
    m_downloadViewMode = mode;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(
        QStringLiteral("layout/downloadViewMode"),
        mode == DownloadViewMode::Compact ? QStringLiteral("compact") : QStringLiteral("grid"));
    emit layoutChanged();
}

} // namespace avar::gui
