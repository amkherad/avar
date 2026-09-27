#pragma once

#include <functional>

#include <QNetworkAccessManager>
#include <QObject>

#if !defined(AVAR_GUI_HOSTING_WASM)
#include <QProcess>
#endif

namespace avar::gui {

class AppSettings;
class GuiPreferences;

struct ExtensionBridgeStatus {
    bool bridgeReachable = false;
    bool extensionConnected = false;
    QString bridgeVersion = QStringLiteral("0.1.0");
    int protocolVersion = 1;
    QString extensionVersion;
};

class ExtensionBridgeClient final : public QObject {
    Q_OBJECT

public:
    static constexpr int kDefaultPort = 18766;

    explicit ExtensionBridgeClient(AppSettings &settings,
                                   GuiPreferences &guiPreferences,
                                   QObject *parent = nullptr);
    ~ExtensionBridgeClient() override;

    void ensureBridgeProcess();
    void syncSettings();
    void pingBridge();
    void requestStatus(const std::function<void(ExtensionBridgeStatus)> &callback);

    [[nodiscard]] QString bridgeBaseUrl() const;

signals:
    void bridgeReachableChanged(bool reachable);

private:
    AppSettings &m_settings;
    GuiPreferences &m_guiPreferences;
    QNetworkAccessManager m_network;
#if !defined(AVAR_GUI_HOSTING_WASM)
    QProcess m_process;
#endif
};

} // namespace avar::gui
