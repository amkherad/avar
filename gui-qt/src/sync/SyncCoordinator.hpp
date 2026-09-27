#pragma once

#include "api/DaemonTypes.hpp"

#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include <memory>

#if defined(AVAR_GUI_HAS_WEBSOCKETS)
#include <QWebSocket>
#endif

namespace avar::gui {

class AppSettings;
class DaemonClient;
class GuiPreferences;
class EventsSseClient;

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
};

class SyncCoordinator final : public QObject {
    Q_OBJECT

public:
    SyncCoordinator(DaemonClient &client, AppSettings &settings, GuiPreferences &guiPreferences,
                    QObject *parent = nullptr);
    ~SyncCoordinator() override;

    void start();
    void stop();

    [[nodiscard]] ConnectionState connectionState() const;

signals:
    void connectionStateChanged(ConnectionState state);
    void snapshotReceived(const SnapshotPayload &payload);

private:
    void reconnectWebSocket();
    void startSseSync();
    void startPollFallback();
    void refreshLists();
    void handleStreamJson(const QJsonObject &root);
    void setState(ConnectionState state);
    void applyTimingFromPreferences();

    DaemonClient &m_client;
    AppSettings &m_settings;
    GuiPreferences &m_guiPreferences;
    std::unique_ptr<EventsSseClient> m_sse;
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    QWebSocket m_socket;
#endif
    QTimer m_pingTimer;
    QTimer m_pollTimer;
    ConnectionState m_state = ConnectionState::Disconnected;
};

} // namespace avar::gui
