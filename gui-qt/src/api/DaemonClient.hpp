#pragma once

#include "api/DaemonTypes.hpp"

#include <functional>

#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QUrl>

namespace avar::gui {

class DaemonApiError {
public:
    DaemonApiError(const QString &message, int code = -1)
        : m_message(message)
        , m_code(code)
    {
    }

    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] int code() const { return m_code; }

private:
    QString m_message;
    int m_code;
};

class IDaemonClient {
public:
    virtual ~IDaemonClient() = default;

    virtual void health(std::function<void(bool, HealthInfo)> callback) = 0;
    virtual void rpc(const QString &method,
                     const QJsonObject &params,
                     std::function<void(bool, QJsonValue, QString)> callback) = 0;
    virtual void listDownloads(std::function<void(bool, QVector<DownloadInfo>, QString)> callback) = 0;
    virtual void listQueues(std::function<void(bool, QVector<QueueInfo>, QString)> callback) = 0;
};

class DaemonClient final : public QObject, public IDaemonClient {
    Q_OBJECT

public:
    struct Options {
        QString baseUrl = QStringLiteral("http://127.0.0.1:8000");
        QString authToken;
        bool useRelativeApi = false;
    };

    explicit DaemonClient(const Options &options, QObject *parent = nullptr);

    void setOptions(const Options &options);
    [[nodiscard]] Options options() const;

    [[nodiscard]] QUrl rpcUrl() const;
    [[nodiscard]] QUrl healthUrl() const;
    [[nodiscard]] QUrl statsUrl() const;
    [[nodiscard]] QUrl webSocketUrl(bool wantsSystemStats = false) const;

    void health(std::function<void(bool, HealthInfo)> callback) override;
    void rpc(const QString &method,
             const QJsonObject &params,
             std::function<void(bool, QJsonValue, QString)> callback) override;
    void listDownloads(std::function<void(bool, QVector<DownloadInfo>, QString)> callback) override;
    void listQueues(std::function<void(bool, QVector<QueueInfo>, QString)> callback) override;

    void addDownload(const QString &url, const QString &queueName = {});
    void systemStats(std::function<void(bool, SystemStatsInfo)> callback);
    void getLogs(int maxLines,
                   qint64 since,
                   std::function<void(bool, QString, qint64)> callback);

    void ping(bool wantsSystemStats, std::function<void(bool)> callback);
    void cliExec(const QStringList &argv,
                 std::function<void(bool, QJsonObject, QString)> callback);
    void watchDownloadProgress(const QString &id);
    void unwatchDownloadProgress(const QString &id);
    void addQueue(const QJsonObject &params, std::function<void(bool, QString, QString)> callback);
    void removeQueue(const QString &id, bool purgeItems);
    void editQueue(const QString &id, const QJsonObject &patch);
    void startQueue(const QString &id);
    void stopQueue(const QString &id);
    void skipLogCursor(qint64 since, int batchSize, std::function<void(bool, qint64)> callback);
    void pauseDownload(const QString &id, std::function<void(bool, QString)> callback);
    void resumeDownload(const QString &id, std::function<void(bool, QString)> callback);
    void startDownload(const QString &id, std::function<void(bool, QString)> callback);
    void stopDownload(const QString &id, std::function<void(bool, QString)> callback);
    void restartDownload(const QString &id, std::function<void(bool, QString)> callback);
    void dismissResumePrompt(const QString &id, std::function<void(bool, QString)> callback);
    void setDownloadUrl(const QString &id, const QString &url, std::function<void(bool, QString)> callback);
    void setDownloadSource(const QString &id, const QJsonObject &source, std::function<void(bool, QString)> callback);
    void probeDownloadUrl(const QJsonObject &options,
                          std::function<void(bool, QJsonObject, QString)> callback);
    void getDownloadDetails(const QString &id,
                            std::function<void(bool, QJsonObject, QString)> callback);
    void computeDownloadChecksum(const QString &id,
                                 const QString &algorithm,
                                 const QString &expected,
                                 std::function<void(bool, QJsonObject, QString)> callback);
    void resolveDownloadPath(const QString &id,
                             std::function<void(bool, QJsonObject, QString)> callback);
    void removeDownload(const QString &id, bool purgeFiles, std::function<void(bool, QString)> callback);
    void getConfig(const QString &key,
                   const QString &defaultValue,
                   std::function<void(QString)> callback);
    void setConfig(const QString &key, const QString &value, std::function<void(bool, QString)> callback);
    void browseDirectory(const QString &path,
                         std::function<void(bool, QJsonObject, QString)> callback);

private:
    QNetworkRequest authorizedRequest(const QUrl &url) const;
    int nextRequestId();

    Options m_options;
    QNetworkAccessManager m_network;
    int m_requestId = 1;
};

} // namespace avar::gui
