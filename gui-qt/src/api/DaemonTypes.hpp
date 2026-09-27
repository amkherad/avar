#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace avar::gui {

struct HealthInfo {
    QString status;
    int queueCount = 0;
    int activeDownloads = 0;
    qint64 uptimeSeconds = 0;
};

struct SystemStatsInfo {
    QString status;
    qint64 diskTotalBytes = 0;
    qint64 diskFreeBytes = 0;
    qint64 memoryTotalBytes = 0;
    qint64 memoryUsedBytes = 0;
    double memoryUsedPercent = 0;
    double cpuUsagePercent = 0;
    qint64 networkRxBytesPerSec = 0;
    qint64 networkTxBytesPerSec = 0;
};

struct DownloadInfo {
    QString id;
    QString name;
    QString status;
    QString queueId;
    qint64 totalBytes = -1;
    qint64 downloadedBytes = 0;
    double progress = 0.0;
};

struct QueueInfo {
    QString id;
    QString name;
    QString description;
    bool running = false;
};

struct SnapshotPayload {
    QVector<DownloadInfo> downloads;
    QVector<QueueInfo> queues;
    HealthInfo health;
    SystemStatsInfo stats;
    bool hasHealth = false;
    bool hasStats = false;
};

} // namespace avar::gui
