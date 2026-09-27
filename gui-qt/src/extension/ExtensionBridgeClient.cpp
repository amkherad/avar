#include "extension/ExtensionBridgeClient.hpp"

#include "config/AppSettings.hpp"
#include "config/GuiPreferences.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QStandardPaths>

namespace avar::gui {

ExtensionBridgeClient::ExtensionBridgeClient(AppSettings &settings,
                                             GuiPreferences &guiPreferences,
                                             QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_guiPreferences(guiPreferences)
{
    connect(&m_settings, &AppSettings::daemonConfigChanged, this, [this] { syncSettings(); });
    connect(&m_guiPreferences, &GuiPreferences::preferencesChanged, this, [this] { syncSettings(); });
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
    const bool effectiveEnabled =
        m_settings.browserExtensionEnabled() && !m_guiPreferences.extensionBridgeSuspended();

    QJsonObject body;
    body.insert(QStringLiteral("enabled"), effectiveEnabled);
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
    requestStatus([this](const ExtensionBridgeStatus &status) {
        emit bridgeReachableChanged(status.bridgeReachable);
    });
}

void ExtensionBridgeClient::requestStatus(const std::function<void(ExtensionBridgeStatus)> &callback)
{
    QNetworkRequest statusRequest(QUrl(bridgeBaseUrl() + QStringLiteral("/extension/status")));
    QNetworkReply *statusReply = m_network.get(statusRequest);
    connect(statusReply, &QNetworkReply::finished, this, [this, statusReply, callback] {
        statusReply->deleteLater();

        ExtensionBridgeStatus status;
        const bool statusOk = statusReply->error() == QNetworkReply::NoError
                              && statusReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        if (statusOk) {
            const QJsonObject obj = QJsonDocument::fromJson(statusReply->readAll()).object();
            status.bridgeReachable = true;
            status.extensionConnected = obj.value(QStringLiteral("connected")).toBool();
            status.bridgeVersion = obj.value(QStringLiteral("bridgeVersion")).toString(QStringLiteral("0.1.0"));
            const QString extVer = obj.value(QStringLiteral("lastExtensionVersion")).toString();
            if (!extVer.isEmpty()) {
                status.extensionVersion = extVer;
            }
            if (obj.contains(QStringLiteral("protocolVersion"))) {
                status.protocolVersion = obj.value(QStringLiteral("protocolVersion")).toInt(1);
            }
        }

        if (!status.bridgeReachable) {
            callback(status);
            return;
        }

        QNetworkReply *pingReply =
            m_network.get(QNetworkRequest(QUrl(bridgeBaseUrl() + QStringLiteral("/v1/ping"))));
        connect(pingReply, &QNetworkReply::finished, this, [pingReply, status, callback] {
            pingReply->deleteLater();
            ExtensionBridgeStatus merged = status;
            if (pingReply->error() == QNetworkReply::NoError
                && pingReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
                const QJsonObject ping = QJsonDocument::fromJson(pingReply->readAll()).object();
                if (ping.contains(QStringLiteral("protocolVersion"))) {
                    merged.protocolVersion = ping.value(QStringLiteral("protocolVersion")).toInt(1);
                }
                const QString ver = ping.value(QStringLiteral("bridgeVersion")).toString();
                if (!ver.isEmpty()) {
                    merged.bridgeVersion = ver;
                }
            }
            callback(merged);
        });
    });
}

} // namespace avar::gui
