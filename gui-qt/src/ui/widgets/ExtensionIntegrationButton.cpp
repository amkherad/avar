#include "ui/widgets/ExtensionIntegrationButton.hpp"

#include "extension/ExtensionBridgeClient.hpp"
#include "i18n/Translator.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>

namespace avar::gui {

ExtensionIntegrationButton::ExtensionIntegrationButton(Translator &translator,
                                                       ExtensionBridgeClient &bridge,
                                                       QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_bridge(bridge)
{
    setObjectName(QStringLiteral("AvarExtensionPanel"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *btn = new AvarButton(AvarButtonVariant::Ghost, this);
    btn->setText(QStringLiteral("🧩"));
    btn->setToolTip(m_tr.tr(QStringLiteral("extensionPanel.aria")));

    m_status = new QLabel(m_tr.tr(QStringLiteral("extensionPanel.checking")), this);
    m_status->setObjectName(QStringLiteral("AvarExtensionStatus"));
    layout->addWidget(btn);
    layout->addWidget(m_status);

    m_bridge.ensureBridgeProcess();
    connect(&m_bridge, &ExtensionBridgeClient::bridgeReachableChanged, this, [this](bool ok) {
        m_status->setText(ok ? m_tr.tr(QStringLiteral("extensionPanel.connected"))
                             : m_tr.tr(QStringLiteral("extensionPanel.disconnected")));
    });
    auto *timer = new QTimer(this);
    timer->setInterval(5000);
    connect(timer, &QTimer::timeout, this, &ExtensionIntegrationButton::refreshStatus);
    timer->start();
    refreshStatus();
}

void ExtensionIntegrationButton::refreshStatus()
{
    m_bridge.pingBridge();
}

} // namespace avar::gui
