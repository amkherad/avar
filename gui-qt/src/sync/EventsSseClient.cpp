#include "sync/EventsSseClient.hpp"

#include "api/DaemonClient.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>
#include <QUrlQuery>

namespace avar::gui {

EventsSseClient::EventsSseClient(DaemonClient &client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
}

EventsSseClient::~EventsSseClient()
{
    stop();
}

void EventsSseClient::setWantsSystemStats(bool wants)
{
    m_wantsStats = wants;
}

void EventsSseClient::start()
{
    m_stopping = false;
    openStream();
}

void EventsSseClient::stop()
{
    m_stopping = true;
    m_openSignaled = false;
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_buffer.clear();
}

void EventsSseClient::openStream()
{
    if (m_stopping || m_reply) {
        return;
    }
    m_openSignaled = false;

    QUrl url = m_client.eventsUrl();
    if (m_wantsStats) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("stats"), QStringLiteral("1"));
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");
    if (!m_client.options().authToken.isEmpty()) {
        request.setRawHeader("Authorization",
                             QByteArray("Bearer ") + m_client.options().authToken.toUtf8());
    }

    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &EventsSseClient::consumeBuffer);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        if (m_stopping) {
            return;
        }
        emit connectionLost();
        if (m_reply) {
            m_reply->deleteLater();
            m_reply = nullptr;
        }
        scheduleReconnect();
    });
    connect(m_reply, &QNetworkReply::errorOccurred, this, [this](QNetworkReply::NetworkError) {
        if (!m_stopping) {
            emit connectionLost();
        }
    });
}

void EventsSseClient::signalOpenedOnce()
{
    if (m_openSignaled) {
        return;
    }
    m_openSignaled = true;
    emit connectionOpened();
}

void EventsSseClient::scheduleReconnect()
{
    if (m_stopping) {
        return;
    }
    QTimer::singleShot(2000, this, &EventsSseClient::openStream);
}

void EventsSseClient::consumeBuffer()
{
    if (!m_reply) {
        return;
    }
    if (m_reply->bytesAvailable() > 0) {
        signalOpenedOnce();
    }
    m_buffer.append(m_reply->readAll());

    while (true) {
        const int blockEnd = m_buffer.indexOf("\n\n");
        if (blockEnd < 0) {
            const int blockEndCr = m_buffer.indexOf("\r\n\r\n");
            if (blockEndCr < 0) {
                break;
            }
            const QByteArray block = m_buffer.left(blockEndCr);
            m_buffer.remove(0, blockEndCr + 4);
            for (const QByteArray &line : block.split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (trimmed.startsWith("data:")) {
                    const QByteArray jsonBytes = trimmed.mid(5).trimmed();
                    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
                    if (doc.isObject()) {
                        emit streamJsonReceived(doc.object());
                    }
                }
            }
            continue;
        }
        const QByteArray block = m_buffer.left(blockEnd);
        m_buffer.remove(0, blockEnd + 2);
        for (const QByteArray &line : block.split('\n')) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.startsWith("data:")) {
                const QByteArray jsonBytes = trimmed.mid(5).trimmed();
                const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
                if (doc.isObject()) {
                    emit streamJsonReceived(doc.object());
                }
            }
        }
    }
}

} // namespace avar::gui
