#pragma once

#include <QObject>
#include <QString>

namespace avar::gui {

enum class SyncChannel {
    Poll,
    Sse,
    WebSocket,
};

enum class DetailPanelMode {
    Pinned,
    Inline,
};

enum class DownloadDoubleClickAction {
    OpenFile,
    OpenDetails,
};

enum class ByteDisplayUnit {
    Binary,
    Decimal,
};

enum class TransferRateDisplayUnit {
    BinaryBytes,
    BinaryBits,
};

enum class FooterMonitorDisplay {
    Text,
    Histogram,
};

struct FooterMonitorSettings {
    bool disk = true;
    bool memory = true;
    bool cpu = false;
    bool network = true;
    FooterMonitorDisplay display = FooterMonitorDisplay::Text;
};

class GuiPreferences final : public QObject {
    Q_OBJECT

public:
    explicit GuiPreferences(QObject *parent = nullptr);

    [[nodiscard]] SyncChannel syncChannel() const;
    [[nodiscard]] int refreshIntervalMs() const;
    [[nodiscard]] int pingIntervalMs() const;
    [[nodiscard]] bool notificationsEnabled() const;
    [[nodiscard]] QString localDownloadPath() const;
    [[nodiscard]] DownloadDoubleClickAction downloadDoubleClickAction() const;
    [[nodiscard]] ByteDisplayUnit byteDisplayUnit() const;
    [[nodiscard]] TransferRateDisplayUnit transferRateDisplayUnit() const;
    [[nodiscard]] FooterMonitorSettings footerMonitors() const;
    [[nodiscard]] DetailPanelMode detailPanelMode() const;
    [[nodiscard]] int downloadPageSize() const;
    [[nodiscard]] bool showDownloadCheckboxes() const;
    [[nodiscard]] bool extensionBridgeSuspended() const;

    [[nodiscard]] QString shortcut(const QString &actionId) const;

    void setSyncChannel(SyncChannel channel);
    void setRefreshIntervalMs(int ms);
    void setPingIntervalMs(int ms);
    void setNotificationsEnabled(bool enabled);
    void setLocalDownloadPath(const QString &path);
    void setDownloadDoubleClickAction(DownloadDoubleClickAction action);
    void setByteDisplayUnit(ByteDisplayUnit unit);
    void setTransferRateDisplayUnit(TransferRateDisplayUnit unit);
    void setFooterMonitors(const FooterMonitorSettings &settings);
    void setDetailPanelMode(DetailPanelMode mode);
    void setDownloadPageSize(int size);
    void setShowDownloadCheckboxes(bool show);
    void setExtensionBridgeSuspended(bool suspended);
    void setShortcut(const QString &actionId, const QString &combo);
    void resetShortcutsToDefaults();

signals:
    void preferencesChanged();

private:
    void load();
    void saveFooterMonitors();

    SyncChannel m_syncChannel = SyncChannel::Sse;
    int m_refreshIntervalMs = 3000;
    int m_pingIntervalMs = 2000;
    bool m_notificationsEnabled = true;
    QString m_localDownloadPath;
    DownloadDoubleClickAction m_doubleClick = DownloadDoubleClickAction::OpenDetails;
    ByteDisplayUnit m_byteUnit = ByteDisplayUnit::Binary;
    TransferRateDisplayUnit m_rateUnit = TransferRateDisplayUnit::BinaryBytes;
    FooterMonitorSettings m_footerMonitors;
    DetailPanelMode m_detailPanelMode = DetailPanelMode::Pinned;
    int m_downloadPageSize = 100;
    bool m_showDownloadCheckboxes = true;
    bool m_extensionBridgeSuspended = false;
};

} // namespace avar::gui
