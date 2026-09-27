#include "api/DaemonClient.hpp"

#include "api/JsonRpc.hpp"
#if defined(AVAR_GUI_QT_EMBED_BACKEND)
#include "backend/InMemoryRpc.hpp"
#endif
#include "sync/SnapshotParser.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>

namespace avar::gui {

namespace {

QString trimBaseUrl(const QString &url)
{
    QString trimmed = url.trimmed();
    while (trimmed.endsWith(QLatin1Char('/'))) {
        trimmed.chop(1);
    }
    return trimmed;
}

DownloadInfo parseDownload(const QJsonValue &value)
{
    return parseDownloadItem(value);
}

QueueInfo parseQueue(const QJsonValue &value)
{
    return parseQueueRecord(value);
}

} // namespace

DaemonClient::DaemonClient(const Options &options, QObject *parent)
    : QObject(parent)
    , m_options(options)
{
    m_options.baseUrl = trimBaseUrl(m_options.baseUrl);
}

void DaemonClient::setOptions(const Options &options)
{
    m_options = options;
    m_options.baseUrl = trimBaseUrl(m_options.baseUrl);
}

DaemonClient::Options DaemonClient::options() const
{
    return m_options;
}

void DaemonClient::setInMemoryTransport(bool enabled)
{
    m_inMemoryTransport = enabled;
}

bool DaemonClient::usesInMemoryTransport() const
{
    return m_inMemoryTransport;
}

QUrl DaemonClient::rpcUrl() const
{
    if (m_options.useRelativeApi) {
        return QUrl(QStringLiteral("/api/rpc"));
    }
    return QUrl(m_options.baseUrl + QStringLiteral("/api/rpc"));
}

QUrl DaemonClient::healthUrl() const
{
    if (m_options.useRelativeApi) {
        return QUrl(QStringLiteral("/api/health"));
    }
    return QUrl(m_options.baseUrl + QStringLiteral("/api/health"));
}

QUrl DaemonClient::statsUrl() const
{
    if (m_options.useRelativeApi) {
        return QUrl(QStringLiteral("/api/stats"));
    }
    return QUrl(m_options.baseUrl + QStringLiteral("/api/stats"));
}

QUrl DaemonClient::eventsUrl() const
{
    if (m_options.useRelativeApi) {
        return QUrl(QStringLiteral("/api/events"));
    }
    return QUrl(m_options.baseUrl + QStringLiteral("/api/events"));
}

QUrl DaemonClient::webSocketUrl(bool wantsSystemStats) const
{
    QString url;
    if (m_options.useRelativeApi) {
        url = QStringLiteral("ws://localhost/api/ws");
    } else {
        QString wsBase = m_options.baseUrl;
        wsBase.replace(QStringLiteral("http://"), QStringLiteral("ws://"));
        wsBase.replace(QStringLiteral("https://"), QStringLiteral("wss://"));
        url = wsBase + QStringLiteral("/api/ws");
    }
    if (wantsSystemStats) {
        url += QStringLiteral("?stats=1");
    }
    return QUrl(url);
}

QNetworkRequest DaemonClient::authorizedRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_options.authToken.isEmpty()) {
        request.setRawHeader("Authorization",
                             QByteArray("Bearer ") + m_options.authToken.toUtf8());
    }
    return request;
}

int DaemonClient::nextRequestId()
{
    return ++m_requestId;
}

void DaemonClient::health(std::function<void(bool, HealthInfo)> callback)
{
    if (m_inMemoryTransport) {
#if defined(AVAR_GUI_QT_EMBED_BACKEND)
        rpc(QStringLiteral("health"), {}, [callback](bool ok, const QJsonValue &result, const QString &) {
            if (!ok) {
                callback(false, {});
                return;
            }
            const QJsonObject obj = result.toObject();
            HealthInfo info;
            info.status = obj.value(QStringLiteral("status")).toString();
            info.queueCount = obj.value(QStringLiteral("queueCount")).toInt();
            info.activeDownloads = obj.value(QStringLiteral("activeDownloads")).toInt();
            info.uptimeSeconds = obj.value(QStringLiteral("uptimeSeconds")).toVariant().toLongLong();
            callback(info.status == QStringLiteral("ok"), info);
        });
        return;
#else
        callback(false, {});
        return;
#endif
    }
    QNetworkReply *reply = m_network.get(authorizedRequest(healthUrl()));
    connect(reply, &QNetworkReply::finished, this, [reply, callback] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback(false, {});
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        HealthInfo info;
        info.status = obj.value(QStringLiteral("status")).toString();
        info.queueCount = obj.value(QStringLiteral("queueCount")).toInt();
        info.activeDownloads = obj.value(QStringLiteral("activeDownloads")).toInt();
        info.uptimeSeconds = obj.value(QStringLiteral("uptimeSeconds")).toVariant().toLongLong();
        callback(info.status == QStringLiteral("ok"), info);
    });
}

void DaemonClient::rpc(const QString &method,
                       const QJsonObject &params,
                       std::function<void(bool, QJsonValue, QString)> callback)
{
    const QJsonObject payload = makeRpcRequest(method, params, nextRequestId());
    if (m_inMemoryTransport) {
#if defined(AVAR_GUI_QT_EMBED_BACKEND)
        QByteArray response;
        QString error;
        if (!inMemoryRpcRequest(QJsonDocument(payload).toJson(QJsonDocument::Compact), &response, &error)) {
            callback(false, {}, error);
            return;
        }
        const JsonRpcResponse parsed = parseRpcResponse(response);
        if (!parsed.ok) {
            callback(false, {}, parsed.error.message);
            return;
        }
        callback(true, parsed.result, {});
        return;
#else
        callback(false, {}, QStringLiteral("Embedded backend not built"));
        return;
#endif
    }
    QNetworkReply *reply =
        m_network.post(authorizedRequest(rpcUrl()), QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, callback] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback(false, {}, reply->errorString());
            return;
        }
        const JsonRpcResponse parsed = parseRpcResponse(reply->readAll());
        if (!parsed.ok) {
            callback(false, {}, parsed.error.message);
            return;
        }
        callback(true, parsed.result, {});
    });
}

void DaemonClient::listDownloads(std::function<void(bool, QVector<DownloadInfo>, QString)> callback)
{
    rpc(QStringLiteral("downloads.list"), {}, [callback](bool ok, const QJsonValue &result, const QString &err) {
        if (!ok) {
            callback(false, {}, err);
            return;
        }
        const QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("exitCode")).toInt() != 0) {
            callback(true, {}, {});
            return;
        }
        QVector<DownloadInfo> items;
        const QJsonArray arr = obj.value(QStringLiteral("downloads")).toArray();
        for (const QJsonValue &value : arr) {
            items.push_back(parseDownload(value));
        }
        callback(true, items, {});
    });
}

void DaemonClient::listQueues(std::function<void(bool, QVector<QueueInfo>, QString)> callback)
{
    rpc(QStringLiteral("queue.list"), {}, [callback](bool ok, const QJsonValue &result, const QString &err) {
        if (!ok) {
            callback(false, {}, err);
            return;
        }
        const QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("exitCode")).toInt() != 0) {
            callback(false, {}, QStringLiteral("queue.list failed"));
            return;
        }
        QVector<QueueInfo> items;
        const QJsonArray arr = obj.value(QStringLiteral("queues")).toArray();
        for (const QJsonValue &value : arr) {
            items.push_back(parseQueue(value));
        }
        callback(true, items, {});
    });
}

void DaemonClient::addDownload(const QString &url, const QString &queueName)
{
    QJsonObject params;
    params.insert(QStringLiteral("url"), url);
    params.insert(QStringLiteral("attached"), false);
    if (!queueName.isEmpty()) {
        params.insert(QStringLiteral("queue"), queueName);
    }
    rpc(QStringLiteral("download.add"), params, [](bool, const QJsonValue &, const QString &) {});
}

void DaemonClient::systemStats(std::function<void(bool, SystemStatsInfo)> callback)
{
    if (m_inMemoryTransport) {
#if defined(AVAR_GUI_QT_EMBED_BACKEND)
        rpc(QStringLiteral("system.stats"), {}, [callback](bool ok, const QJsonValue &result, const QString &) {
            if (!ok) {
                callback(false, {});
                return;
            }
            const QJsonObject obj = result.toObject();
            SystemStatsInfo stats;
            stats.status = obj.value(QStringLiteral("status")).toString(QStringLiteral("ok"));
            stats.diskTotalBytes = obj.value(QStringLiteral("diskTotalBytes")).toVariant().toLongLong();
            stats.diskFreeBytes = obj.value(QStringLiteral("diskFreeBytes")).toVariant().toLongLong();
            stats.memoryTotalBytes = obj.value(QStringLiteral("memoryTotalBytes")).toVariant().toLongLong();
            stats.memoryUsedBytes = obj.value(QStringLiteral("memoryUsedBytes")).toVariant().toLongLong();
            stats.memoryUsedPercent = obj.value(QStringLiteral("memoryUsedPercent")).toDouble();
            stats.cpuUsagePercent = obj.value(QStringLiteral("cpuUsagePercent")).toDouble();
            stats.networkRxBytesPerSec =
                obj.value(QStringLiteral("networkRxBytesPerSec")).toVariant().toLongLong();
            stats.networkTxBytesPerSec =
                obj.value(QStringLiteral("networkTxBytesPerSec")).toVariant().toLongLong();
            callback(stats.status == QStringLiteral("ok"), stats);
        });
        return;
#else
        callback(false, {});
        return;
#endif
    }
    QNetworkReply *reply = m_network.get(authorizedRequest(statsUrl()));
    connect(reply, &QNetworkReply::finished, this, [reply, callback] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback(false, {});
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        SystemStatsInfo stats;
        stats.status = obj.value(QStringLiteral("status")).toString();
        stats.diskTotalBytes = obj.value(QStringLiteral("diskTotalBytes")).toVariant().toLongLong();
        stats.diskFreeBytes = obj.value(QStringLiteral("diskFreeBytes")).toVariant().toLongLong();
        stats.memoryTotalBytes = obj.value(QStringLiteral("memoryTotalBytes")).toVariant().toLongLong();
        stats.memoryUsedBytes = obj.value(QStringLiteral("memoryUsedBytes")).toVariant().toLongLong();
        stats.memoryUsedPercent = obj.value(QStringLiteral("memoryUsedPercent")).toDouble();
        stats.cpuUsagePercent = obj.value(QStringLiteral("cpuUsagePercent")).toDouble();
        stats.networkRxBytesPerSec = obj.value(QStringLiteral("networkRxBytesPerSec")).toVariant().toLongLong();
        stats.networkTxBytesPerSec = obj.value(QStringLiteral("networkTxBytesPerSec")).toVariant().toLongLong();
        callback(stats.status == QStringLiteral("ok"), stats);
    });
}

void DaemonClient::getLogs(int maxLines,
                           qint64 since,
                           std::function<void(bool, QString, qint64)> callback)
{
    QJsonObject params;
    params.insert(QStringLiteral("maxLines"), maxLines);
    params.insert(QStringLiteral("since"), since);
    rpc(QStringLiteral("logs.get"), params, [callback, since](bool ok, const QJsonValue &result, const QString &) {
        if (!ok) {
            callback(false, {}, since);
            return;
        }
        const QJsonObject obj = result.toObject();
        const QString logs = obj.value(QStringLiteral("logs")).toString();
        const qint64 next = obj.value(QStringLiteral("nextOffset")).toVariant().toLongLong();
        callback(true, logs, next > 0 ? next : since);
    });
}

} // namespace avar::gui
