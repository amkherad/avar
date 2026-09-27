#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace avar::gui {

struct JsonRpcError {
    int code = -1;
    QString message;
};

struct JsonRpcResponse {
    bool ok = false;
    QJsonValue result;
    JsonRpcError error;
};

QJsonObject makeRpcRequest(const QString &method, const QJsonObject &params, int id);

JsonRpcResponse parseRpcResponse(const QByteArray &body);

} // namespace avar::gui
