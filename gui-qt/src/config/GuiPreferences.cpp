#include "config/GuiPreferences.hpp"

#include <QSettings>

namespace avar::gui {

namespace {

QString defaultShortcut(const QString &id)
{
    static const QHash<QString, QString> defaults = {
        {QStringLiteral("download.add"), QStringLiteral("ctrl+n")},
        {QStringLiteral("download.search"), QStringLiteral("ctrl+f")},
        {QStringLiteral("download.pause"), QStringLiteral("ctrl+p")},
        {QStringLiteral("download.start"), QStringLiteral("ctrl+shift+s")},
        {QStringLiteral("download.stop"), QStringLiteral("ctrl+shift+x")},
        {QStringLiteral("download.delete"), QStringLiteral("delete")},
        {QStringLiteral("nav.dashboard"), QStringLiteral("ctrl+1")},
        {QStringLiteral("nav.settings"), QStringLiteral("ctrl+,")},
        {QStringLiteral("nav.help"), QStringLiteral("f1")},
        {QStringLiteral("console.toggle"), QStringLiteral("ctrl+`")},
        {QStringLiteral("detailPanel.toggle"), QStringLiteral("ctrl+d")},
    };
    return defaults.value(id, QStringLiteral("ctrl+n"));
}

} // namespace

GuiPreferences::GuiPreferences(QObject *parent)
    : QObject(parent)
{
    load();
}

void GuiPreferences::load()
{
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    const QString sync = store.value(QStringLiteral("gui/syncChannel"), QStringLiteral("sse")).toString();
    if (sync == QStringLiteral("poll")) {
        m_syncChannel = SyncChannel::Poll;
    } else if (sync == QStringLiteral("websocket")) {
        m_syncChannel = SyncChannel::WebSocket;
    } else {
        m_syncChannel = SyncChannel::Sse;
    }
    m_refreshIntervalMs = store.value(QStringLiteral("gui/refreshIntervalMs"), 3000).toInt();
    m_pingIntervalMs = store.value(QStringLiteral("gui/pingIntervalMs"), 2000).toInt();
    m_notificationsEnabled = store.value(QStringLiteral("gui/notificationsEnabled"), true).toBool();
    m_localDownloadPath = store.value(QStringLiteral("gui/localDownloadPath")).toString();
    m_doubleClick = store.value(QStringLiteral("gui/downloadDoubleClick"), QStringLiteral("openDetails"))
                        == QStringLiteral("openFile")
                        ? DownloadDoubleClickAction::OpenFile
                        : DownloadDoubleClickAction::OpenDetails;
    m_byteUnit = store.value(QStringLiteral("gui/byteDisplayUnit"), QStringLiteral("binary"))
                     == QStringLiteral("decimal")
                     ? ByteDisplayUnit::Decimal
                     : ByteDisplayUnit::Binary;
    m_rateUnit = store.value(QStringLiteral("gui/transferRateDisplayUnit"), QStringLiteral("binary-bytes"))
                     == QStringLiteral("binary-bits")
                     ? TransferRateDisplayUnit::BinaryBits
                     : TransferRateDisplayUnit::BinaryBytes;
    m_footerMonitors.disk = store.value(QStringLiteral("gui/footer/disk"), true).toBool();
    m_footerMonitors.memory = store.value(QStringLiteral("gui/footer/memory"), true).toBool();
    m_footerMonitors.cpu = store.value(QStringLiteral("gui/footer/cpu"), false).toBool();
    m_footerMonitors.network = store.value(QStringLiteral("gui/footer/network"), true).toBool();
    m_footerMonitors.display = store.value(QStringLiteral("gui/footer/display"), QStringLiteral("text"))
                                   == QStringLiteral("histogram")
                                   ? FooterMonitorDisplay::Histogram
                                   : FooterMonitorDisplay::Text;
    m_detailPanelMode = store.value(QStringLiteral("gui/detailPanelMode"), QStringLiteral("pinned"))
                            == QStringLiteral("inline")
                            ? DetailPanelMode::Inline
                            : DetailPanelMode::Pinned;
    m_downloadPageSize = store.value(QStringLiteral("gui/downloadPageSize"), 100).toInt();
    m_showDownloadCheckboxes = store.value(QStringLiteral("gui/showDownloadCheckboxes"), true).toBool();
    m_extensionBridgeSuspended = store.value(QStringLiteral("browserExtension/suspended"), false).toBool();
}

SyncChannel GuiPreferences::syncChannel() const
{
    return m_syncChannel;
}

int GuiPreferences::refreshIntervalMs() const
{
    return m_refreshIntervalMs;
}

int GuiPreferences::pingIntervalMs() const
{
    return m_pingIntervalMs;
}

bool GuiPreferences::notificationsEnabled() const
{
    return m_notificationsEnabled;
}

QString GuiPreferences::localDownloadPath() const
{
    return m_localDownloadPath;
}

DownloadDoubleClickAction GuiPreferences::downloadDoubleClickAction() const
{
    return m_doubleClick;
}

ByteDisplayUnit GuiPreferences::byteDisplayUnit() const
{
    return m_byteUnit;
}

TransferRateDisplayUnit GuiPreferences::transferRateDisplayUnit() const
{
    return m_rateUnit;
}

FooterMonitorSettings GuiPreferences::footerMonitors() const
{
    return m_footerMonitors;
}

DetailPanelMode GuiPreferences::detailPanelMode() const
{
    return m_detailPanelMode;
}

int GuiPreferences::downloadPageSize() const
{
    return m_downloadPageSize;
}

bool GuiPreferences::showDownloadCheckboxes() const
{
    return m_showDownloadCheckboxes;
}

bool GuiPreferences::extensionBridgeSuspended() const
{
    return m_extensionBridgeSuspended;
}

QString GuiPreferences::shortcut(const QString &actionId) const
{
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    return store.value(QStringLiteral("shortcuts/") + actionId, defaultShortcut(actionId)).toString();
}

void GuiPreferences::setSyncChannel(SyncChannel channel)
{
    m_syncChannel = channel;
    QString value = QStringLiteral("sse");
    if (channel == SyncChannel::Poll) {
        value = QStringLiteral("poll");
    } else if (channel == SyncChannel::WebSocket) {
        value = QStringLiteral("websocket");
    }
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("gui/syncChannel"), value);
    emit preferencesChanged();
}

void GuiPreferences::setRefreshIntervalMs(int ms)
{
    m_refreshIntervalMs = qMax(1000, ms);
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("gui/refreshIntervalMs"),
                                                                        m_refreshIntervalMs);
    emit preferencesChanged();
}

void GuiPreferences::setPingIntervalMs(int ms)
{
    m_pingIntervalMs = qMax(1000, ms);
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("gui/pingIntervalMs"),
                                                                          m_pingIntervalMs);
    emit preferencesChanged();
}

void GuiPreferences::setNotificationsEnabled(bool enabled)
{
    m_notificationsEnabled = enabled;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("gui/notificationsEnabled"),
                                                                        enabled);
    emit preferencesChanged();
}

void GuiPreferences::setLocalDownloadPath(const QString &path)
{
    m_localDownloadPath = path;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("gui/localDownloadPath"), path);
    emit preferencesChanged();
}

void GuiPreferences::setDownloadDoubleClickAction(DownloadDoubleClickAction action)
{
    m_doubleClick = action;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(
        QStringLiteral("gui/downloadDoubleClick"),
        action == DownloadDoubleClickAction::OpenFile ? QStringLiteral("openFile") : QStringLiteral("openDetails"));
    emit preferencesChanged();
}

void GuiPreferences::setByteDisplayUnit(ByteDisplayUnit unit)
{
    m_byteUnit = unit;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(
        QStringLiteral("gui/byteDisplayUnit"),
        unit == ByteDisplayUnit::Decimal ? QStringLiteral("decimal") : QStringLiteral("binary"));
    emit preferencesChanged();
}

void GuiPreferences::setTransferRateDisplayUnit(TransferRateDisplayUnit unit)
{
    m_rateUnit = unit;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(
        QStringLiteral("gui/transferRateDisplayUnit"),
        unit == TransferRateDisplayUnit::BinaryBits ? QStringLiteral("binary-bits") : QStringLiteral("binary-bytes"));
    emit preferencesChanged();
}

void GuiPreferences::setFooterMonitors(const FooterMonitorSettings &settings)
{
    m_footerMonitors = settings;
    saveFooterMonitors();
    emit preferencesChanged();
}

void GuiPreferences::setDetailPanelMode(DetailPanelMode mode)
{
    m_detailPanelMode = mode;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(
        QStringLiteral("gui/detailPanelMode"),
        mode == DetailPanelMode::Inline ? QStringLiteral("inline") : QStringLiteral("pinned"));
    emit preferencesChanged();
}

void GuiPreferences::setDownloadPageSize(int size)
{
    m_downloadPageSize = qBound(10, size, 500);
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("gui/downloadPageSize"),
                                                                          m_downloadPageSize);
    emit preferencesChanged();
}

void GuiPreferences::setShowDownloadCheckboxes(bool show)
{
    m_showDownloadCheckboxes = show;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("gui/showDownloadCheckboxes"),
                                                                          show);
    emit preferencesChanged();
}

void GuiPreferences::setExtensionBridgeSuspended(bool suspended)
{
    m_extensionBridgeSuspended = suspended;
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("browserExtension/suspended"),
                                                                          suspended);
    emit preferencesChanged();
}

void GuiPreferences::setShortcut(const QString &actionId, const QString &combo)
{
    QSettings(QStringLiteral("Avar"), QStringLiteral("gui-qt")).setValue(QStringLiteral("shortcuts/") + actionId, combo);
    emit preferencesChanged();
}

void GuiPreferences::resetShortcutsToDefaults()
{
    const QStringList ids = {QStringLiteral("download.add"), QStringLiteral("download.search"),
                               QStringLiteral("download.pause"), QStringLiteral("download.start"),
                               QStringLiteral("download.stop"), QStringLiteral("download.delete"),
                               QStringLiteral("nav.dashboard"), QStringLiteral("nav.settings"),
                               QStringLiteral("nav.help"), QStringLiteral("console.toggle"),
                               QStringLiteral("detailPanel.toggle")};
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    for (const QString &id : ids) {
        store.setValue(QStringLiteral("shortcuts/") + id, defaultShortcut(id));
    }
    emit preferencesChanged();
}

void GuiPreferences::saveFooterMonitors()
{
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    store.setValue(QStringLiteral("gui/footer/disk"), m_footerMonitors.disk);
    store.setValue(QStringLiteral("gui/footer/memory"), m_footerMonitors.memory);
    store.setValue(QStringLiteral("gui/footer/cpu"), m_footerMonitors.cpu);
    store.setValue(QStringLiteral("gui/footer/network"), m_footerMonitors.network);
    store.setValue(QStringLiteral("gui/footer/display"),
                   m_footerMonitors.display == FooterMonitorDisplay::Histogram ? QStringLiteral("histogram")
                                                                               : QStringLiteral("text"));
}

} // namespace avar::gui
