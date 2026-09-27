#include "extension/ExtensionBridgeClient.hpp"

#include "config/AppSettings.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QStandardPaths>

namespace avar::gui {

ExtensionBridgeClient::ExtensionBridgeClient(AppSettings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    connect(&m_settings, &AppSettings::daemonConfigChanged, this, [this] { syncSettings(); });
}

ExtensionBridgeClient::~ExtensionBridgeClient()
{
#if !defined(AVAR_GUI_HOSTING_WASM)
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        m_process.waitForFinished(2000);
    }
#endif
}

QString ExtensionBridgeClient::bridgeBaseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(kDefaultPort);
}

void ExtensionBridgeClient::ensureBridgeProcess()
{
#if defined(AVAR_GUI_HOSTING_WASM)
    pingBridge();
#elif defined(AVAR_GUI_HOSTING_DESKTOP) && defined(AVAR_GUI_QT_ENABLE_EXTENSION_SUBPROCESS)
    if (m_process.state() != QProcess::NotRunning) {
        syncSettings();
        return;
    }

    const QString localBin =
        QCoreApplication::applicationDirPath() + QStringLiteral("/avar-extension-daemon");
    const QString devCandidate = QDir(QCoreApplication::applicationDirPath())
                                     .absoluteFilePath(QStringLiteral("../../extensions/daemon/build/avar-extension-daemon"));
    QString program;
    if (QFileInfo::exists(localBin)) {
        program = localBin;
    } else if (QFileInfo::exists(devCandidate)) {
        program = devCandidate;
    } else {
        pingBridge();
        return;
    }

    m_process.setProgram(program);
    m_process.setArguments({QStringLiteral("--port"), QString::number(kDefaultPort)});
    m_process.start();
    connect(&m_process, &QProcess::started, this, [this] { syncSettings(); });
#else
    pingBridge();
#endif
}

void ExtensionBridgeClient::syncSettings()
{
    if (!m_settings.browserExtensionEnabled()) {
        return;
    }

    QJsonObject body;
    body.insert(QStringLiteral("enabled"), m_settings.browserExtensionEnabled());
    body.insert(QStringLiteral("daemonUrl"), m_settings.daemonBaseUrl());
    const QString token = m_settings.authToken();
    if (!token.isEmpty()) {
        body.insert(QStringLiteral("authToken"), token);
    }

    QNetworkRequest request(QUrl(bridgeBaseUrl() + QStringLiteral("/extension/settings")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply =
        m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void ExtensionBridgeClient::pingBridge()
{
    QNetworkReply *reply = m_network.get(QNetworkRequest(QUrl(bridgeBaseUrl() + QStringLiteral("/v1/ping"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        emit bridgeReachableChanged(ok);
    });
}

} // namespace avar::gui
