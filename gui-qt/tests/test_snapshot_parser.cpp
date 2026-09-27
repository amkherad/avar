#include "sync/SnapshotParser.hpp"

#include <QJsonObject>
#include <QtTest>

using namespace avar::gui;

class SnapshotParserTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesDownloadFilename();
    void detectsUnchanged();
};

void SnapshotParserTest::parsesDownloadFilename()
{
    QJsonObject item;
    item.insert(QStringLiteral("id"), QStringLiteral("d1"));
    item.insert(QStringLiteral("filename"), QStringLiteral("file.bin"));
    item.insert(QStringLiteral("status"), QStringLiteral("downloading"));
    const DownloadInfo info = parseDownloadItem(item);
    QCOMPARE(info.id, QStringLiteral("d1"));
    QCOMPARE(info.name, QStringLiteral("file.bin"));
    QCOMPARE(info.status, QStringLiteral("downloading"));
}

void SnapshotParserTest::detectsUnchanged()
{
    QJsonObject root;
    root.insert(QStringLiteral("type"), QStringLiteral("unchanged"));
    QVERIFY(isUnchangedStreamPayload(root));
}

QTEST_MAIN(SnapshotParserTest)
#include "test_snapshot_parser.moc"
