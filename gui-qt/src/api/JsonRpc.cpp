#include "api/JsonRpc.hpp"

#include <QJsonDocument>
#include <QJsonParseError>

namespace avar::gui {

QJsonObject makeRpcRequest(const QString &method, const QJsonObject &params, int id)
{
    QJsonObject root;
    root.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    root.insert(QStringLiteral("method"), method);
    root.insert(QStringLiteral("params"), params);
    root.insert(QStringLiteral("id"), id);
    return root;
}

JsonRpcResponse parseRpcResponse(const QByteArray &body)
{
    JsonRpcResponse out;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error.code = -32700;
        out.error.message = QStringLiteral("Invalid JSON-RPC payload");
        return out;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("error"))) {
        const QJsonObject err = obj.value(QStringLiteral("error")).toObject();
        out.error.code = err.value(QStringLiteral("code")).toInt(-1);
        out.error.message = err.value(QStringLiteral("message")).toString(QStringLiteral("RPC error"));
        return out;
    }

    if (!obj.contains(QStringLiteral("result"))) {
        out.error.code = -32603;
        out.error.message = QStringLiteral("RPC response missing result");
        return out;
    }

    out.ok = true;
    out.result = obj.value(QStringLiteral("result"));
    return out;
}

} // namespace avar::gui
