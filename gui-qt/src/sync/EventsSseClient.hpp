#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

class QNetworkReply;

namespace avar::gui {

class DaemonClient;

class EventsSseClient final : public QObject {
    Q_OBJECT

public:
    explicit EventsSseClient(DaemonClient &client, QObject *parent = nullptr);
    ~EventsSseClient() override;

    void setWantsSystemStats(bool wants);
    void start();
    void stop();

signals:
    void streamJsonReceived(const QJsonObject &payload);
    void connectionOpened();
    void connectionLost();

private:
    void openStream();
    void scheduleReconnect();
    void consumeBuffer();

    DaemonClient &m_client;
    ::QNetworkAccessManager m_network;
    ::QNetworkReply *m_reply = nullptr;
    ::QByteArray m_buffer;
    bool m_wantsStats = false;
    bool m_stopping = false;
};

} // namespace avar::gui
