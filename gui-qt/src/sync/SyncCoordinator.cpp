#include "sync/SyncCoordinator.hpp"

#include "api/DaemonClient.hpp"
#include "config/AppSettings.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
#include <QNetworkRequest>
#endif

namespace avar::gui {

SyncCoordinator::SyncCoordinator(DaemonClient &client, AppSettings &settings, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_settings(settings)
{
    m_pingTimer.setInterval(5000);
    connect(&m_pingTimer, &QTimer::timeout, this, [this] {
        m_client.health([this](bool ok, HealthInfo) {
            setState(ok ? ConnectionState::Connected : ConnectionState::Disconnected);
        });
    });

    m_pollTimer.setInterval(3000);
    connect(&m_pollTimer, &QTimer::timeout, this, [this] { refreshLists(); });

#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    connect(&m_socket, &QWebSocket::connected, this, [this] {
        setState(ConnectionState::Connected);
        m_pollTimer.stop();
    });

    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        setState(ConnectionState::Disconnected);
        startPollFallback();
        QTimer::singleShot(1500, this, [this] { reconnectWebSocket(); });
    });

    connect(&m_socket, &QWebSocket::textMessageReceived, this, [this](const QString &message) {
        const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        const QJsonObject root = doc.object();
        if (root.value(QStringLiteral("type")).toString() != QStringLiteral("snapshot")) {
            return;
        }
        refreshLists();
    });
#endif

    connect(&m_settings, &AppSettings::daemonConfigChanged, this, [this] {
        stop();
        DaemonClient::Options opts = m_client.options();
        opts.baseUrl = m_settings.daemonBaseUrl();
        opts.authToken = m_settings.authToken();
        opts.useRelativeApi = m_settings.useRelativeDaemonApi();
        m_client.setOptions(opts);
        start();
    });
}

SyncCoordinator::~SyncCoordinator()
{
    stop();
}

void SyncCoordinator::start()
{
    setState(ConnectionState::Connecting);
    m_pingTimer.start();
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    reconnectWebSocket();
#endif
    startPollFallback();
}

void SyncCoordinator::stop()
{
    m_pingTimer.stop();
    m_pollTimer.stop();
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
    }
#endif
}

ConnectionState SyncCoordinator::connectionState() const
{
    return m_state;
}

void SyncCoordinator::reconnectWebSocket()
{
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        return;
    }
    const QUrl url = m_client.webSocketUrl(false);
    QNetworkRequest request(url);
    if (!m_client.options().authToken.isEmpty()) {
        request.setRawHeader("Authorization",
                             QByteArray("Bearer ") + m_client.options().authToken.toUtf8());
    }
    m_socket.open(request);
#endif
}

void SyncCoordinator::startPollFallback()
{
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
    }
}

void SyncCoordinator::refreshLists()
{
    if (m_state != ConnectionState::Connected && m_state != ConnectionState::Connecting) {
        return;
    }
    m_client.listDownloads([this](bool okDownloads, QVector<DownloadInfo> downloads, QString) {
        SnapshotPayload payload;
        if (okDownloads) {
            payload.downloads = std::move(downloads);
        }
        m_client.listQueues([this, payload](bool okQueues, QVector<QueueInfo> queues, QString) mutable {
            if (okQueues) {
                payload.queues = std::move(queues);
            }
            emit snapshotReceived(payload);
        });
    });
}

void SyncCoordinator::setState(ConnectionState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit connectionStateChanged(state);
}

} // namespace avar::gui
