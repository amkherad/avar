#pragma once

#include "theme/ThemeTokens.hpp"

#include <QObject>
#include <QSettings>

namespace avar::gui {

class AppSettings final : public QObject {
    Q_OBJECT

public:
    explicit AppSettings(QObject *parent = nullptr);

    [[nodiscard]] QString daemonBaseUrl() const;
    [[nodiscard]] QString authToken() const;
    [[nodiscard]] bool useRelativeDaemonApi() const;
    [[nodiscard]] ThemeSetting themeSetting() const;
    [[nodiscard]] QString locale() const;
    [[nodiscard]] bool browserExtensionEnabled() const;
    [[nodiscard]] int sidebarWidth() const;
    [[nodiscard]] bool keepInTrayOnClose() const;

    void setDaemonBaseUrl(const QString &url);
    void setAuthToken(const QString &token);
    void setThemeSetting(ThemeSetting setting);
    void setLocale(const QString &locale);
    void setBrowserExtensionEnabled(bool enabled);
    void setSidebarWidth(int width);
    void setKeepInTrayOnClose(bool enabled);

signals:
    void daemonConfigChanged();
    void themeSettingChanged();
    void localeChanged();
    void desktopBehaviorChanged();

private:
    QSettings m_store;
};

} // namespace avar::gui
