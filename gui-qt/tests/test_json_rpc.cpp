#include "api/JsonRpc.hpp"

#include <QtTest>

using namespace avar::gui;

class JsonRpcTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesSuccess();
    void parsesError();
};

void JsonRpcTest::parsesSuccess()
{
    const QByteArray body = QByteArrayLiteral(
        R"({"jsonrpc":"2.0","result":{"exitCode":0},"id":1})");
    const JsonRpcResponse response = parseRpcResponse(body);
    QVERIFY(response.ok);
    QCOMPARE(response.result.toObject().value(QStringLiteral("exitCode")).toInt(), 0);
}

void JsonRpcTest::parsesError()
{
    const QByteArray body =
        QByteArrayLiteral(R"({"jsonrpc":"2.0","error":{"code":-1,"message":"fail"},"id":2})");
    const JsonRpcResponse response = parseRpcResponse(body);
    QVERIFY(!response.ok);
    QCOMPARE(response.error.message, QStringLiteral("fail"));
}

QTEST_MAIN(JsonRpcTest)
#include "test_json_rpc.moc"
