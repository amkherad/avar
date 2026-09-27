#include "api/DaemonClient.hpp"

#include <QJsonArray>

namespace avar::gui {

namespace {

void rpcObject(DaemonClient *self,
               const QString &method,
               const QJsonObject &params,
               const std::function<void(bool, QJsonObject, QString)> &callback)
{
    self->rpc(method, params, [callback](bool ok, const QJsonValue &result, const QString &err) {
        callback(ok, result.toObject(), err);
    });
}

} // namespace

namespace {

void cliExecCheck(DaemonClient *self,
                  const QStringList &argv,
                  std::function<void(bool, QString)> callback)
{
    self->cliExec(argv, [callback](bool ok, const QJsonObject &result, const QString &err) {
        if (!ok) {
            callback(false, err);
            return;
        }
        const int exitCode = result.value(QStringLiteral("exitCode")).toInt();
        if (exitCode != 0) {
            callback(false, result.value(QStringLiteral("stderr")).toString());
            return;
        }
        callback(true, {});
    });
}

} // namespace

void DaemonClient::ping(bool wantsSystemStats, std::function<void(bool)> callback)
{
    if (wantsSystemStats) {
        systemStats([callback](bool ok, const SystemStatsInfo &stats) {
            callback(ok && stats.status == QStringLiteral("ok"));
        });
        return;
    }
    health([callback](bool ok, const HealthInfo &health) {
        callback(ok && health.status == QStringLiteral("ok"));
    });
}

void DaemonClient::cliExec(const QStringList &argv,
                           std::function<void(bool, QJsonObject, QString)> callback)
{
    QJsonArray arr;
    for (const QString &part : argv) {
        arr.append(part);
    }
    QJsonObject params;
    params.insert(QStringLiteral("argv"), arr);
    rpc(QStringLiteral("cli.exec"), params, [callback](bool ok, const QJsonValue &result, const QString &err) {
        if (!ok) {
            callback(false, {}, err);
            return;
        }
        callback(true, result.toObject(), {});
    });
}

void DaemonClient::watchDownloadProgress(const QString &id)
{
    rpc(QStringLiteral("download.watch"), {{QStringLiteral("id"), id}}, [](bool, const QJsonValue &, const QString &) {});
}

void DaemonClient::unwatchDownloadProgress(const QString &id)
{
    rpc(QStringLiteral("download.unwatch"), {{QStringLiteral("id"), id}}, [](bool, const QJsonValue &, const QString &) {});
}

void DaemonClient::addQueue(const QJsonObject &params,
                          std::function<void(bool, QString, QString)> callback)
{
    rpc(QStringLiteral("queue.add"), params, [callback](bool ok, const QJsonValue &result, const QString &err) {
        if (!ok) {
            callback(false, {}, err);
            return;
        }
        const QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("exitCode")).toInt() != 0) {
            callback(false, {}, QStringLiteral("queue.add failed"));
            return;
        }
        callback(true, obj.value(QStringLiteral("id")).toString(), {});
    });
}

void DaemonClient::removeQueue(const QString &id, bool purgeItems)
{
    QJsonObject params;
    params.insert(QStringLiteral("id"), id);
    params.insert(QStringLiteral("purgeItems"), purgeItems);
    rpc(QStringLiteral("queue.remove"), params, [](bool, const QJsonValue &, const QString &) {});
}

void DaemonClient::editQueue(const QString &id, const QJsonObject &patch)
{
    QJsonObject params = patch;
    params.insert(QStringLiteral("id"), id);
    rpc(QStringLiteral("queue.edit"), params, [](bool, const QJsonValue &, const QString &) {});
}

void DaemonClient::startQueue(const QString &id)
{
    rpc(QStringLiteral("queue.start"), {{QStringLiteral("id"), id}}, [](bool, const QJsonValue &, const QString &) {});
}

void DaemonClient::stopQueue(const QString &id)
{
    rpc(QStringLiteral("queue.stop"), {{QStringLiteral("id"), id}}, [](bool, const QJsonValue &, const QString &) {});
}

void DaemonClient::skipLogCursor(qint64 since, int batchSize, std::function<void(bool, qint64)> callback)
{
    getLogs(batchSize, since, [this, since, batchSize, callback](bool ok, const QString &logs, qint64 next) {
        if (!ok) {
            callback(false, since);
            return;
        }
        if (next <= since || logs.trimmed().isEmpty()) {
            callback(true, next > since ? next : since);
            return;
        }
        skipLogCursor(next, batchSize, callback);
    });
}

void DaemonClient::pauseDownload(const QString &id, std::function<void(bool, QString)> callback)
{
    cliExecCheck(this, {QStringLiteral("avar"), QStringLiteral("dl"), QStringLiteral("pause"), id}, callback);
}

void DaemonClient::resumeDownload(const QString &id, std::function<void(bool, QString)> callback)
{
    cliExecCheck(this, {QStringLiteral("avar"), QStringLiteral("dl"), QStringLiteral("resume"), id}, callback);
}

void DaemonClient::startDownload(const QString &id, std::function<void(bool, QString)> callback)
{
    cliExecCheck(this, {QStringLiteral("avar"), QStringLiteral("dl"), QStringLiteral("start"), id}, callback);
}

void DaemonClient::stopDownload(const QString &id, std::function<void(bool, QString)> callback)
{
    cliExecCheck(this, {QStringLiteral("avar"), QStringLiteral("dl"), QStringLiteral("stop"), id}, callback);
}

void DaemonClient::restartDownload(const QString &id, std::function<void(bool, QString)> callback)
{
    cliExecCheck(this, {QStringLiteral("avar"), QStringLiteral("dl"), QStringLiteral("restart"), id}, callback);
}

void DaemonClient::dismissResumePrompt(const QString &id, std::function<void(bool, QString)> callback)
{
    cliExecCheck(this,
                 {QStringLiteral("avar"), QStringLiteral("dl"), QStringLiteral("dismiss-resume"), id},
                 callback);
}

void DaemonClient::setDownloadUrl(const QString &id,
                                  const QString &url,
                                  std::function<void(bool, QString)> callback)
{
    rpc(QStringLiteral("download.setUrl"),
        {{QStringLiteral("id"), id}, {QStringLiteral("url"), url}},
        [callback](bool ok, const QJsonValue &result, const QString &err) {
            if (!ok) {
                callback(false, err);
                return;
            }
            const int code = result.toObject().value(QStringLiteral("exitCode")).toInt();
            callback(code == 0, code != 0 ? QStringLiteral("download.setUrl failed") : QString());
        });
}

void DaemonClient::setDownloadSource(const QString &id,
                                     const QJsonObject &source,
                                     std::function<void(bool, QString)> callback)
{
    QJsonObject params = source;
    params.insert(QStringLiteral("id"), id);
    rpc(QStringLiteral("download.setSource"), params, [callback](bool ok, const QJsonValue &result, const QString &err) {
        if (!ok) {
            callback(false, err);
            return;
        }
        const int code = result.toObject().value(QStringLiteral("exitCode")).toInt();
        callback(code == 0, code != 0 ? QStringLiteral("download.setSource failed") : QString());
    });
}

void DaemonClient::probeDownloadUrl(const QJsonObject &options,
                                    std::function<void(bool, QJsonObject, QString)> callback)
{
    rpcObject(this, QStringLiteral("download.probe"), options, callback);
}

void DaemonClient::getDownloadDetails(const QString &id,
                                      std::function<void(bool, QJsonObject, QString)> callback)
{
    rpcObject(this, QStringLiteral("download.getDetails"), {{QStringLiteral("id"), id}}, callback);
}

void DaemonClient::computeDownloadChecksum(const QString &id,
                                           const QString &algorithm,
                                           const QString &expected,
                                           std::function<void(bool, QJsonObject, QString)> callback)
{
    QJsonObject params;
    params.insert(QStringLiteral("id"), id);
    params.insert(QStringLiteral("algorithm"), algorithm);
    if (!expected.isEmpty()) {
        params.insert(QStringLiteral("expected"), expected);
    }
    rpcObject(this, QStringLiteral("download.checksum"), params, callback);
}

void DaemonClient::resolveDownloadPath(const QString &id,
                                       std::function<void(bool, QJsonObject, QString)> callback)
{
    rpcObject(this, QStringLiteral("download.resolvePath"), {{QStringLiteral("id"), id}}, callback);
}

void DaemonClient::removeDownload(const QString &id,
                                  bool purgeFiles,
                                  std::function<void(bool, QString)> callback)
{
    QStringList argv = {QStringLiteral("avar"), QStringLiteral("dl"), QStringLiteral("rm"), id,
                        QStringLiteral("--force")};
    if (purgeFiles) {
        argv.append(QStringLiteral("--purge-files"));
    }
    cliExecCheck(this, argv, callback);
}

void DaemonClient::getConfig(const QString &key,
                             const QString &defaultValue,
                             std::function<void(QString)> callback)
{
    QStringList argv = {QStringLiteral("avar"), QStringLiteral("config"), QStringLiteral("get"), key};
    if (!defaultValue.isEmpty()) {
        argv.append(QStringLiteral("--defaultValue=%1").arg(defaultValue));
    }
    cliExec(argv, [callback, defaultValue](bool ok, const QJsonObject &result, const QString &) {
        if (!ok || result.value(QStringLiteral("exitCode")).toInt() != 0) {
            callback(defaultValue);
            return;
        }
        const QString output = result.value(QStringLiteral("output")).toString().trimmed();
        callback(output.isEmpty() ? defaultValue : output);
    });
}

void DaemonClient::setConfig(const QString &key,
                             const QString &value,
                             std::function<void(bool, QString)> callback)
{
    cliExecCheck(this,
                 {QStringLiteral("avar"), QStringLiteral("config"), QStringLiteral("set"), key, value},
                 callback);
}

void DaemonClient::browseDirectory(const QString &path,
                                   std::function<void(bool, QJsonObject, QString)> callback)
{
    rpcObject(this, QStringLiteral("fs.browse"), {{QStringLiteral("path"), path}}, callback);
}

} // namespace avar::gui
