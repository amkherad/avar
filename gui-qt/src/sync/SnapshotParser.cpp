#include "sync/SnapshotParser.hpp"

#include <QJsonArray>
#include <QJsonObject>

namespace avar::gui {

namespace {

qint64 toLongLong(const QJsonValue &value)
{
    if (value.isDouble()) {
        return static_cast<qint64>(value.toDouble());
    }
    if (value.isString()) {
        return value.toString().toLongLong();
    }
    return 0;
}

double toDouble(const QJsonValue &value)
{
    if (value.isDouble()) {
        return value.toDouble();
    }
    if (value.isString()) {
        return value.toString().toDouble();
    }
    return 0.0;
}

HealthInfo parseHealth(const QJsonObject &obj)
{
    HealthInfo health;
    health.status = obj.value(QStringLiteral("status")).toString();
    health.queueCount = obj.value(QStringLiteral("queueCount")).toInt();
    health.activeDownloads = obj.value(QStringLiteral("activeDownloads")).toInt();
    health.uptimeSeconds = toLongLong(obj.value(QStringLiteral("uptimeSeconds")));
    return health;
}

SystemStatsInfo parseSystemStatsObject(const QJsonObject &record)
{
    SystemStatsInfo stats;
    stats.status = record.value(QStringLiteral("status")).toString();
    stats.diskTotalBytes = toLongLong(record.value(QStringLiteral("diskTotalBytes")));
    stats.diskFreeBytes = toLongLong(record.value(QStringLiteral("diskFreeBytes")));
    stats.memoryTotalBytes = toLongLong(record.value(QStringLiteral("memoryTotalBytes")));
    stats.memoryUsedBytes = toLongLong(record.value(QStringLiteral("memoryUsedBytes")));
    stats.memoryUsedPercent = toDouble(record.value(QStringLiteral("memoryUsedPercent")));
    stats.cpuUsagePercent = toDouble(record.value(QStringLiteral("cpuUsagePercent")));
    stats.networkRxBytesPerSec = toLongLong(record.value(QStringLiteral("networkRxBytesPerSec")));
    stats.networkTxBytesPerSec = toLongLong(record.value(QStringLiteral("networkTxBytesPerSec")));
    return stats;
}

} // namespace

bool isUnchangedStreamPayload(const QJsonValue &raw)
{
    if (!raw.isObject()) {
        return false;
    }
    return raw.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("unchanged");
}

std::optional<SystemStatsInfo> parseStreamStatsPayload(const QJsonValue &raw)
{
    if (!raw.isObject()) {
        return std::nullopt;
    }
    const QJsonObject record = raw.toObject();
    if (record.value(QStringLiteral("type")).toString() != QStringLiteral("stats")) {
        return std::nullopt;
    }
    return parseSystemStatsObject(record);
}

DownloadInfo parseDownloadItem(const QJsonValue &item)
{
    DownloadInfo info;
    const QJsonObject record = item.toObject();
    info.id = record.value(QStringLiteral("id")).toString();
    const QString filename = record.value(QStringLiteral("filename")).toString();
    info.name = filename.isEmpty() ? record.value(QStringLiteral("name")).toString() : filename;
    if (info.name.isEmpty()) {
        info.name = QStringLiteral("—");
    }
    QString status = record.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("failed")) {
        status = QStringLiteral("error");
    }
    info.status = status.isEmpty() ? QStringLiteral("unknown") : status;
    info.queueId = record.value(QStringLiteral("queueId")).toString();
    if (info.queueId.isEmpty()) {
        info.queueId = record.value(QStringLiteral("queue")).toString();
    }
    info.totalBytes = toLongLong(record.value(QStringLiteral("totalBytes")));
    info.downloadedBytes = toLongLong(record.value(QStringLiteral("bytesDownloaded")));
    if (info.downloadedBytes == 0) {
        info.downloadedBytes = toLongLong(record.value(QStringLiteral("downloadedBytes")));
    }
    info.progress = record.value(QStringLiteral("progress")).toDouble();
    if (info.progress <= 0 && info.totalBytes > 0) {
        info.progress = static_cast<double>(info.downloadedBytes) / static_cast<double>(info.totalBytes);
    }
    return info;
}

QueueInfo parseQueueRecord(const QJsonValue &item)
{
    QueueInfo info;
    const QJsonObject record = item.toObject();
    info.id = record.value(QStringLiteral("id")).toString();
    info.name = record.value(QStringLiteral("name")).toString();
    info.description = record.value(QStringLiteral("description")).toString();
    info.running = record.value(QStringLiteral("started")).toBool(record.value(QStringLiteral("running")).toBool());
    return info;
}

std::optional<SnapshotPayload> parseSnapshotPayload(const QJsonValue &raw)
{
    if (!raw.isObject()) {
        return std::nullopt;
    }
    const QJsonObject record = raw.toObject();
    SnapshotPayload payload;
    if (record.contains(QStringLiteral("health"))) {
        payload.health = parseHealth(record.value(QStringLiteral("health")).toObject());
        payload.hasHealth = true;
    }
    if (record.contains(QStringLiteral("stats"))) {
        payload.stats = parseSystemStatsObject(record.value(QStringLiteral("stats")).toObject());
        payload.hasStats = true;
    }
    const QJsonArray downloads = record.value(QStringLiteral("downloads")).toArray();
    for (const QJsonValue &value : downloads) {
        payload.downloads.push_back(parseDownloadItem(value));
    }
    const QJsonArray queues = record.value(QStringLiteral("queues")).toArray();
    for (const QJsonValue &value : queues) {
        payload.queues.push_back(parseQueueRecord(value));
    }
    return payload;
}

} // namespace avar::gui
