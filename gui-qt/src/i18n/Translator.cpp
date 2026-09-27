#include "i18n/Translator.hpp"

#include <QHash>

namespace avar::gui {

Translator::Translator(QObject *parent)
    : QObject(parent)
{
}

void Translator::setLocale(const QString &locale)
{
    m_locale = locale;
}

bool Translator::isRtl() const
{
    return m_locale == QStringLiteral("fa");
}

QString Translator::tr(const QString &key) const
{
    static const QHash<QString, QHash<QString, QString>> table = {
        {QStringLiteral("en"),
         {
             {QStringLiteral("app.title"), QStringLiteral("Avar")},
             {QStringLiteral("app.subtitle"), QStringLiteral("Download Manager")},
             {QStringLiteral("nav.dashboard"), QStringLiteral("Dashboard")},
             {QStringLiteral("nav.settings"), QStringLiteral("Settings")},
             {QStringLiteral("nav.help"), QStringLiteral("Help")},
             {QStringLiteral("nav.back"), QStringLiteral("Back to dashboard")},
             {QStringLiteral("session.connected"), QStringLiteral("Connected")},
             {QStringLiteral("session.disconnected"), QStringLiteral("Disconnected")},
             {QStringLiteral("session.connecting"), QStringLiteral("Connecting…")},
             {QStringLiteral("session.add"), QStringLiteral("Add session")},
             {QStringLiteral("download.title"), QStringLiteral("Downloads")},
             {QStringLiteral("download.add"), QStringLiteral("Add download")},
             {QStringLiteral("download.batchAdd.button"), QStringLiteral("Batch add")},
             {QStringLiteral("download.urlLabel"), QStringLiteral("URL")},
             {QStringLiteral("download.searchPlaceholder"), QStringLiteral("Search downloads…")},
             {QStringLiteral("download.selectHint"), QStringLiteral("Select a download to view details.")},
             {QStringLiteral("download.detailsTitle"), QStringLiteral("Download details")},
             {QStringLiteral("console.title"), QStringLiteral("Console")},
             {QStringLiteral("console.clear"), QStringLiteral("Clear")},
             {QStringLiteral("health.uptime"), QStringLiteral("Uptime")},
             {QStringLiteral("downloads.empty"), QStringLiteral("No downloads in this queue")},
             {QStringLiteral("settings.general"), QStringLiteral("General")},
             {QStringLiteral("settings.title"), QStringLiteral("Settings")},
             {QStringLiteral("settings.categories.general"), QStringLiteral("🌐 General")},
             {QStringLiteral("settings.categories.downloads"), QStringLiteral("📥 Downloads")},
             {QStringLiteral("settings.categories.queues"), QStringLiteral("📋 Queues")},
             {QStringLiteral("settings.categories.daemon"), QStringLiteral("🖥️ Daemon")},
             {QStringLiteral("settings.categories.browser"), QStringLiteral("🧩 Browser integration")},
             {QStringLiteral("settings.categories.shortcuts"), QStringLiteral("⌨️ Shortcuts")},
             {QStringLiteral("settings.categories.about"), QStringLiteral("✨ About")},
             {QStringLiteral("queue.title"), QStringLiteral("Queues")},
             {QStringLiteral("queue.add"), QStringLiteral("Add queue")},
             {QStringLiteral("queue.nameLabel"), QStringLiteral("Queue name")},
             {QStringLiteral("dialog.cancel"), QStringLiteral("Cancel")},
             {QStringLiteral("dialog.add"), QStringLiteral("Add")},
             {QStringLiteral("dialog.confirm"), QStringLiteral("Confirm")},
             {QStringLiteral("download.batchAdd.placeholder"), QStringLiteral("One URL per line")},
             {QStringLiteral("theme.lightSoft"), QStringLiteral("Soft light")},
             {QStringLiteral("theme.lightBright"), QStringLiteral("Bright light")},
             {QStringLiteral("theme.dark"), QStringLiteral("Dark")},
             {QStringLiteral("theme.system"), QStringLiteral("System")},
             {QStringLiteral("theme.toggle"), QStringLiteral("Toggle light/dark")},
             {QStringLiteral("settings.general.locale"), QStringLiteral("Language code")},
             {QStringLiteral("settings.downloads.hint"), QStringLiteral("Default download folder is read from daemon config.")},
             {QStringLiteral("settings.downloads.defaultPath"), QStringLiteral("Default path")},
             {QStringLiteral("settings.queues.hint"), QStringLiteral("Queue defaults are managed in the daemon config.")},
             {QStringLiteral("settings.browser.enableExtension"), QStringLiteral("Enable browser extension bridge")},
             {QStringLiteral("settings.shortcuts.hint"), QStringLiteral("Global shortcuts require desktop integration (planned).")},
             {QStringLiteral("settings.about.backendLoading"), QStringLiteral("Loading backend version…")},
             {QStringLiteral("settings.about.backendUnknown"), QStringLiteral("Unknown")},
             {QStringLiteral("session.baseUrl"), QStringLiteral("Server URL")},
             {QStringLiteral("session.authToken"), QStringLiteral("Auth token (optional)")},
             {QStringLiteral("parity.panelPending"), QStringLiteral("This section is not ported yet; see gui-qt/PARITY.md.")},
             {QStringLiteral("help.welcome"), QStringLiteral("Help documentation will mirror gui/docs here.")},
         }},
        {QStringLiteral("fa"),
         {
             {QStringLiteral("app.title"), QStringLiteral("آوار")},
             {QStringLiteral("app.subtitle"), QStringLiteral("مدیر دانلود")},
             {QStringLiteral("nav.dashboard"), QStringLiteral("داشبورد")},
             {QStringLiteral("nav.settings"), QStringLiteral("تنظیمات")},
             {QStringLiteral("nav.help"), QStringLiteral("راهنما")},
             {QStringLiteral("nav.back"), QStringLiteral("بازگشت به داشبورد")},
             {QStringLiteral("session.connected"), QStringLiteral("متصل")},
             {QStringLiteral("session.disconnected"), QStringLiteral("قطع")},
             {QStringLiteral("downloads.empty"), QStringLiteral("دانلودی در این صف نیست")},
             {QStringLiteral("settings.general"), QStringLiteral("عمومی")},
             {QStringLiteral("help.welcome"), QStringLiteral("مستندات راهنما از gui/docs هم‌تراز خواهد شد.")},
         }},
    };

    const auto localeTable = table.value(m_locale, table.value(QStringLiteral("en")));
    return localeTable.value(key, key);
}

} // namespace avar::gui
