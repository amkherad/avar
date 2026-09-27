#pragma once

#include <QObject>

namespace avar::gui {

enum class DownloadViewMode {
    Grid,
    Compact,
};

class LayoutPreferences final : public QObject {
    Q_OBJECT

public:
    explicit LayoutPreferences(QObject *parent = nullptr);

    [[nodiscard]] int sidebarWidth() const;
    [[nodiscard]] int consoleHeight() const;
    [[nodiscard]] int detailPanelWidth() const;
    [[nodiscard]] bool detailPanelOpen() const;
    [[nodiscard]] bool consoleOpen() const;
    [[nodiscard]] DownloadViewMode downloadViewMode() const;

    void setSidebarWidth(int width);
    void adjustSidebarWidth(int delta);
    void setConsoleHeight(int height);
    void adjustConsoleHeight(int delta);
    void setDetailPanelWidth(int width);
    void adjustDetailPanelWidth(int delta);
    void setDetailPanelOpen(bool open);
    void setConsoleOpen(bool open);
    void setDownloadViewMode(DownloadViewMode mode);

signals:
    void layoutChanged();

private:
    int m_sidebarWidth = 260;
    int m_consoleHeight = 220;
    int m_detailPanelWidth = 300;
    bool m_detailPanelOpen = false;
    bool m_consoleOpen = false;
    DownloadViewMode m_downloadViewMode = DownloadViewMode::Grid;
};

} // namespace avar::gui
