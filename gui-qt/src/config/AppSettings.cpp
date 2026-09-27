#include "config/AppSettings.hpp"

#include "core/Hosting.hpp"

namespace avar::gui {

namespace {

ThemeSetting readThemeSetting(const QSettings &store)
{
    const QString raw = store.value(QStringLiteral("theme"), QStringLiteral("system")).toString();
    if (raw == QStringLiteral("dark")) {
        return ThemeSetting::Dark;
    }
    if (raw == QStringLiteral("light-bright")) {
        return ThemeSetting::LightBright;
    }
    if (raw == QStringLiteral("light")) {
        return ThemeSetting::LightSoft;
    }
    return ThemeSetting::System;
}

QString writeThemeSetting(ThemeSetting setting)
{
    switch (setting) {
    case ThemeSetting::Dark:
        return QStringLiteral("dark");
    case ThemeSetting::LightBright:
        return QStringLiteral("light-bright");
    case ThemeSetting::LightSoft:
        return QStringLiteral("light");
    case ThemeSetting::System:
        return QStringLiteral("system");
    }
    return QStringLiteral("system");
}

} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_store(QStringLiteral("Avar"), QStringLiteral("gui-qt"))
{
}

QString AppSettings::daemonBaseUrl() const
{
    if (hostingUsesRelativeDaemonApi(detectHostingMode())) {
        return {};
    }
    return m_store
        .value(QStringLiteral("daemon/baseUrl"), QStringLiteral("http://127.0.0.1:8000"))
        .toString();
}

QString AppSettings::authToken() const
{
    return m_store.value(QStringLiteral("daemon/authToken")).toString();
}

bool AppSettings::useRelativeDaemonApi() const
{
    return hostingUsesRelativeDaemonApi(detectHostingMode());
}

ThemeSetting AppSettings::themeSetting() const
{
    return readThemeSetting(m_store);
}

QString AppSettings::locale() const
{
    return m_store.value(QStringLiteral("locale"), QStringLiteral("en")).toString();
}

bool AppSettings::browserExtensionEnabled() const
{
    return m_store.value(QStringLiteral("browserExtension/enabled"), true).toBool();
}

int AppSettings::sidebarWidth() const
{
    return m_store.value(QStringLiteral("layout/sidebarWidth"), 280).toInt();
}

void AppSettings::setDaemonBaseUrl(const QString &url)
{
    m_store.setValue(QStringLiteral("daemon/baseUrl"), url);
    emit daemonConfigChanged();
}

void AppSettings::setAuthToken(const QString &token)
{
    m_store.setValue(QStringLiteral("daemon/authToken"), token);
    emit daemonConfigChanged();
}

void AppSettings::setThemeSetting(ThemeSetting setting)
{
    m_store.setValue(QStringLiteral("theme"), writeThemeSetting(setting));
    emit themeSettingChanged();
}

void AppSettings::setLocale(const QString &locale)
{
    m_store.setValue(QStringLiteral("locale"), locale);
    emit localeChanged();
}

void AppSettings::setBrowserExtensionEnabled(bool enabled)
{
    m_store.setValue(QStringLiteral("browserExtension/enabled"), enabled);
    emit daemonConfigChanged();
}

void AppSettings::setSidebarWidth(int width)
{
    m_store.setValue(QStringLiteral("layout/sidebarWidth"), width);
}

} // namespace avar::gui
