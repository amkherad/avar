#include "sync/SyncCoordinator.hpp"

#include "api/DaemonClient.hpp"
#include "config/AppSettings.hpp"
#include "config/GuiPreferences.hpp"
#include "sync/EventsSseClient.hpp"
#include "sync/SnapshotParser.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
#include <QNetworkRequest>
#endif

namespace avar::gui {

SyncCoordinator::SyncCoordinator(DaemonClient &client, AppSettings &settings, GuiPreferences &guiPreferences,
                                 QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_settings(settings)
    , m_guiPreferences(guiPreferences)
{
    m_sse = std::make_unique<EventsSseClient>(client, this);
    applyTimingFromPreferences();
    connect(&m_pingTimer, &QTimer::timeout, this, [this] { runReachabilityCheck(); });

    connect(&m_pollTimer, &QTimer::timeout, this, [this] { refreshLists(); });

    connect(&m_guiPreferences, &GuiPreferences::preferencesChanged, this, [this] { applyTimingFromPreferences(); });

    connect(m_sse.get(), &EventsSseClient::connectionOpened, this, [this] {
        setState(ConnectionState::Connected);
        m_pollTimer.stop();
    });
    connect(m_sse.get(), &EventsSseClient::connectionLost, this, [this] {
        startPollFallback();
    });
    connect(m_sse.get(), &EventsSseClient::streamJsonReceived, this, [this](const QJsonObject &obj) {
        setState(ConnectionState::Connected);
        handleStreamJson(obj);
    });

#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    connect(&m_socket, &QWebSocket::connected, this, [this] {
        setState(ConnectionState::Connected);
        m_pollTimer.stop();
        m_sse->stop();
    });

    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        startPollFallback();
        startSseSync();
        runReachabilityCheck();
        QTimer::singleShot(1500, this, [this] { reconnectWebSocket(); });
    });

    connect(&m_socket, &QWebSocket::textMessageReceived, this, [this](const QString &message) {
        const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        if (doc.isObject()) {
            handleStreamJson(doc.object());
        }
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
    runReachabilityCheck();
    if (m_client.usesInMemoryTransport()) {
        startPollFallback();
        setState(ConnectionState::Connected);
        return;
    }
    startSseSync();
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    reconnectWebSocket();
#endif
    startPollFallback();
}

void SyncCoordinator::stop()
{
    m_pingTimer.stop();
    m_pollTimer.stop();
    if (m_sse) {
        m_sse->stop();
    }
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

void SyncCoordinator::startSseSync()
{
    m_sse->setWantsSystemStats(true);
    m_sse->start();
}

void SyncCoordinator::reconnectWebSocket()
{
#if defined(AVAR_GUI_HAS_WEBSOCKETS)
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        return;
    }
    const QUrl url = m_client.webSocketUrl(true);
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

void SyncCoordinator::handleStreamJson(const QJsonObject &root)
{
    const QJsonValue raw = root;
    if (isUnchangedStreamPayload(raw)) {
        return;
    }

    if (const std::optional<SystemStatsInfo> stats = parseStreamStatsPayload(raw)) {
        SnapshotPayload payload;
        payload.stats = *stats;
        payload.hasStats = true;
        emit snapshotReceived(payload);
        return;
    }

    if (const std::optional<SnapshotPayload> parsed = parseSnapshotPayload(raw)) {
        SnapshotPayload payload = *parsed;
        if (root.value(QStringLiteral("type")).toString() == QStringLiteral("snapshot")) {
            emit snapshotReceived(payload);
            return;
        }
        if (!payload.downloads.isEmpty() || !payload.queues.isEmpty()) {
            emit snapshotReceived(payload);
        }
    }
}

void SyncCoordinator::runReachabilityCheck()
{
    m_client.health([this](bool okHealth, HealthInfo) {
        if (okHealth) {
            setState(ConnectionState::Connected);
            return;
        }
        m_client.listDownloads([this](bool okDownloads, QVector<DownloadInfo>, QString) {
            if (okDownloads) {
                setState(ConnectionState::Connected);
                return;
            }
            if (m_state == ConnectionState::Connected) {
                setState(ConnectionState::Disconnected);
            }
        });
    });
}

void SyncCoordinator::refreshLists()
{
    m_client.listDownloads([this](bool okDownloads, QVector<DownloadInfo> downloads, QString) {
        if (okDownloads) {
            setState(ConnectionState::Connected);
        } else if (m_state == ConnectionState::Connected) {
            setState(ConnectionState::Disconnected);
        }

        SnapshotPayload payload;
        if (okDownloads) {
            payload.downloads = std::move(downloads);
        }
        m_client.listQueues([this, payload](bool okQueues, QVector<QueueInfo> queues, QString) mutable {
            if (okQueues) {
                setState(ConnectionState::Connected);
                payload.queues = std::move(queues);
            } else if (m_state == ConnectionState::Connected && payload.downloads.isEmpty()) {
                setState(ConnectionState::Disconnected);
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

void SyncCoordinator::applyTimingFromPreferences()
{
    m_pingTimer.setInterval(m_guiPreferences.pingIntervalMs());
    m_pollTimer.setInterval(m_guiPreferences.refreshIntervalMs());
}

} // namespace avar::gui
