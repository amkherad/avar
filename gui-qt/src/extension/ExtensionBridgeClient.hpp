#pragma once

#include <QNetworkAccessManager>
#include <QObject>

#if !defined(AVAR_GUI_HOSTING_WASM)
#include <QProcess>
#endif

namespace avar::gui {

class AppSettings;

class ExtensionBridgeClient final : public QObject {
    Q_OBJECT

public:
    static constexpr int kDefaultPort = 18766;

    explicit ExtensionBridgeClient(AppSettings &settings, QObject *parent = nullptr);
    ~ExtensionBridgeClient() override;

    void ensureBridgeProcess();
    void syncSettings();

    [[nodiscard]] QString bridgeBaseUrl() const;

signals:
    void bridgeReachableChanged(bool reachable);

private:
    void pingBridge();

    AppSettings &m_settings;
    QNetworkAccessManager m_network;
#if !defined(AVAR_GUI_HOSTING_WASM)
    QProcess m_process;
#endif
};

} // namespace avar::gui
