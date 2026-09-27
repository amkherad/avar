#pragma once

#include "api/DaemonTypes.hpp"

#include <QObject>
#include <QTimer>

#if defined(AVAR_GUI_HAS_WEBSOCKETS)
#include <QWebSocket>
#endif

namespace avar::gui {

class AppSettings;
class DaemonClient;

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
};

class SyncCoordinator final : public QObject {
    Q_OBJECT

public:
    SyncCoordinator(DaemonClient &client, AppSettings &settings, QObject *parent = nullptr);
    ~SyncCoordinator() override;

    void start();
    void stop();

    [[nodiscard]] ConnectionState connectionState() const;

signals:
    void connectionStateChanged(ConnectionState state);
    void snapshotReceived(const SnapshotPayload &payload);

private:
    void reconnectWebSocket();
    void startPollFallback();
    void refreshLists();
    void setState(ConnectionState state);

    DaemonClient &m_client;
    AppSettings &m_settings;
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    QWebSocket m_socket;
#endif
    QTimer m_pingTimer;
    QTimer m_pollTimer;
    ConnectionState m_state = ConnectionState::Disconnected;
};

} // namespace avar::gui
