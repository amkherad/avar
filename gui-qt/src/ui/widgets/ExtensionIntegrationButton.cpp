#include "ui/widgets/ExtensionIntegrationButton.hpp"

#include "config/AppSettings.hpp"
#include "config/GuiPreferences.hpp"
#include "extension/ExtensionBridgeClient.hpp"
#include "theme/ThemeManager.hpp"
#include "i18n/Translator.hpp"
#include "theme/FaIcon.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QCheckBox>
#include <QClipboard>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QStyle>
#include <QTimer>
#include <QEvent>
#include <QSizePolicy>
#include <QPalette>
#include <QVBoxLayout>

namespace avar::gui {

namespace {

constexpr const char *kBundledExtensionVersion = "0.1.0";

QLabel *metaLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("class", QStringLiteral("AvarExtensionMetaKey"));
    return label;
}

QLabel *metaValue(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("class", QStringLiteral("AvarExtensionMetaValue"));
    label->setWordWrap(true);
    return label;
}

} // namespace

ExtensionIntegrationButton::ExtensionIntegrationButton(Translator &translator,
                                                       ThemeManager &theme,
                                                       ExtensionBridgeClient &bridge,
                                                       AppSettings &appSettings,
                                                       GuiPreferences &guiPreferences,
                                                       QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_theme(theme)
    , m_bridge(bridge)
    , m_appSettings(appSettings)
    , m_guiPreferences(guiPreferences)
{
    setObjectName(QStringLiteral("AvarExtensionPanel"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_triggerHost = new QWidget(this);
    m_triggerHost->setProperty("class", QStringLiteral("AvarExtensionTrigger"));
    m_triggerHost->setCursor(Qt::PointingHandCursor);
    m_triggerHost->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *triggerGrid = new QGridLayout(m_triggerHost);
    triggerGrid->setContentsMargins(0, 0, 0, 0);

    m_trigger = new AvarButton(AvarButtonVariant::Ghost, m_triggerHost);
    m_trigger->setIconSize(QSize(14, 14));
    m_trigger->setFocusPolicy(Qt::NoFocus);
    m_trigger->setToolTip(m_tr.tr(QStringLiteral("extensionPanel.aria")));

    m_dot = new QLabel(m_triggerHost);
    m_dot->setProperty("class", QStringLiteral("AvarExtensionDot"));
    m_dot->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_dot->setFixedSize(7, 7);

    triggerGrid->addWidget(m_trigger, 0, 0, Qt::AlignCenter);
    triggerGrid->addWidget(m_dot, 0, 0, Qt::AlignTop | Qt::AlignRight);

    layout->addWidget(m_triggerHost);

    buildMenu();

    connect(m_trigger, &QPushButton::clicked, this, &ExtensionIntegrationButton::toggleMenu);

    m_bridge.ensureBridgeProcess();
    connect(&m_bridge, &ExtensionBridgeClient::bridgeReachableChanged, this, [this](bool ok) {
        m_bridgeReachable = ok;
        if (!m_checking) {
            updateAppearance();
            updateMenuContent();
        }
    });
    connect(&m_appSettings, &AppSettings::daemonConfigChanged, this, [this] {
        m_bridge.syncSettings();
        updateAppearance();
        updateMenuContent();
    });
    connect(&m_guiPreferences, &GuiPreferences::preferencesChanged, this, [this] {
        updateAppearance();
        updateMenuContent();
    });

    auto *timer = new QTimer(this);
    timer->setInterval(5000);
    connect(timer, &QTimer::timeout, this, &ExtensionIntegrationButton::refreshStatus);
    timer->start();

    updateAppearance();
    refreshStatus();

    connect(&m_tr, &Translator::translationsChanged, this, &ExtensionIntegrationButton::retranslateUi);
    connect(&m_theme, &ThemeManager::themeChanged, this, [this] { updateAppearance(); });
}

void ExtensionIntegrationButton::retranslateUi()
{
    m_trigger->setToolTip(m_tr.tr(QStringLiteral("extensionPanel.aria")));
    if (m_listenCheck != nullptr) {
        m_listenCheck->setText(m_tr.tr(QStringLiteral("extensionPanel.listen")));
    }
    if (m_suspendBtn != nullptr) {
        m_suspendBtn->setText(m_tr.tr(QStringLiteral("extensionPanel.suspend")));
    }
    if (m_resumeBtn != nullptr) {
        m_resumeBtn->setText(m_tr.tr(QStringLiteral("extensionPanel.resume")));
    }
    if (m_copyBtn != nullptr) {
        m_copyBtn->setText(m_tr.tr(QStringLiteral("extensionPanel.copyUrl")));
    }
    updateAppearance();
    updateMenuContent();
}

void ExtensionIntegrationButton::buildMenu()
{
    m_menuPopup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_menuPopup->setObjectName(QStringLiteral("AvarExtensionMenu"));
    m_menuPopup->installEventFilter(this);

    auto *root = new QVBoxLayout(m_menuPopup);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *heading = new QLabel(m_tr.tr(QStringLiteral("extensionPanel.title")), m_menuPopup);
    heading->setProperty("class", QStringLiteral("AvarExtensionHeading"));
    root->addWidget(heading);

    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(6);
    m_menuStatusDot = new QLabel(m_menuPopup);
    m_menuStatusDot->setProperty("class", QStringLiteral("AvarExtensionDot"));
    m_menuStatusDot->setFixedSize(8, 8);
    m_menuStatusLabel = new QLabel(m_menuPopup);
    m_menuStatusLabel->setProperty("class", QStringLiteral("AvarExtensionStatusText"));
    statusRow->addWidget(m_menuStatusDot, 0, Qt::AlignVCenter);
    statusRow->addWidget(m_menuStatusLabel, 1, Qt::AlignVCenter);
    root->addLayout(statusRow);

    m_listenCheck = new QCheckBox(m_tr.tr(QStringLiteral("extensionPanel.listen")), m_menuPopup);
    m_listenCheck->setProperty("class", QStringLiteral("AvarExtensionListen"));
    connect(m_listenCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_appSettings.setBrowserExtensionEnabled(checked);
        m_bridge.syncSettings();
        updateMenuContent();
    });
    root->addWidget(m_listenCheck);

    m_suspendSection = new QWidget(m_menuPopup);
    auto *suspendLayout = new QVBoxLayout(m_suspendSection);
    suspendLayout->setContentsMargins(0, 0, 0, 0);
    suspendLayout->setSpacing(6);

    auto *suspendActions = new QHBoxLayout();
    m_suspendBtn = new AvarButton(AvarButtonVariant::Secondary, m_suspendSection);
    m_suspendBtn->setText(m_tr.tr(QStringLiteral("extensionPanel.suspend")));
    m_resumeBtn = new AvarButton(AvarButtonVariant::Primary, m_suspendSection);
    m_resumeBtn->setText(m_tr.tr(QStringLiteral("extensionPanel.resume")));
    connect(m_suspendBtn, &QPushButton::clicked, this, [this] {
        m_guiPreferences.setExtensionBridgeSuspended(true);
        m_bridge.syncSettings();
        updateMenuContent();
    });
    connect(m_resumeBtn, &QPushButton::clicked, this, [this] {
        m_guiPreferences.setExtensionBridgeSuspended(false);
        m_bridge.syncSettings();
        refreshStatus();
        updateMenuContent();
    });
    suspendActions->addWidget(m_suspendBtn);
    suspendActions->addWidget(m_resumeBtn);
    suspendLayout->addLayout(suspendActions);

    m_suspendHint = new QLabel(m_suspendSection);
    m_suspendHint->setProperty("class", QStringLiteral("AvarExtensionSuspendHint"));
    m_suspendHint->setWordWrap(true);
    suspendLayout->addWidget(m_suspendHint);
    root->addWidget(m_suspendSection);

    auto *urlLabel = new QLabel(m_tr.tr(QStringLiteral("extensionPanel.bridgeUrl")), m_menuPopup);
    urlLabel->setProperty("class", QStringLiteral("AvarExtensionFieldLabel"));
    root->addWidget(urlLabel);

    m_bridgeUrlCode = new QLabel(m_menuPopup);
    m_bridgeUrlCode->setProperty("class", QStringLiteral("AvarExtensionCode"));
    m_bridgeUrlCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_bridgeUrlCode->setWordWrap(true);
    root->addWidget(m_bridgeUrlCode);

    m_copyBtn = new AvarButton(AvarButtonVariant::Secondary, m_menuPopup);
    m_copyBtn->setText(m_tr.tr(QStringLiteral("extensionPanel.copyUrl")));
    connect(m_copyBtn, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(m_bridge.bridgeBaseUrl());
        m_copyBtn->setText(m_tr.tr(QStringLiteral("common.copied")));
        QTimer::singleShot(1500, this, [this] {
            m_copyBtn->setText(m_tr.tr(QStringLiteral("extensionPanel.copyUrl")));
        });
    });
    root->addWidget(m_copyBtn, 0, Qt::AlignLeft);

    auto *meta = new QGridLayout();
    meta->setHorizontalSpacing(12);
    meta->setVerticalSpacing(8);
    meta->addWidget(metaLabel(m_tr.tr(QStringLiteral("extensionPanel.bridgeVersion")), m_menuPopup), 0, 0);
    m_bridgeVersionValue = metaValue(QString(), m_menuPopup);
    meta->addWidget(m_bridgeVersionValue, 1, 0);
    meta->addWidget(metaLabel(m_tr.tr(QStringLiteral("extensionPanel.protocolVersion")), m_menuPopup), 0, 1);
    m_protocolVersionValue = metaValue(QString(), m_menuPopup);
    meta->addWidget(m_protocolVersionValue, 1, 1);
    meta->addWidget(metaLabel(m_tr.tr(QStringLiteral("extensionPanel.extensionVersion")), m_menuPopup), 2, 0);
    m_extensionVersionValue = metaValue(QString(), m_menuPopup);
    meta->addWidget(m_extensionVersionValue, 3, 0);
    meta->addWidget(metaLabel(m_tr.tr(QStringLiteral("extensionPanel.bundledVersion")), m_menuPopup), 2, 1);
    m_bundledVersionValue = metaValue(QString::fromUtf8(kBundledExtensionVersion), m_menuPopup);
    meta->addWidget(m_bundledVersionValue, 3, 1);
    root->addLayout(meta);

    m_updateLabel = new QLabel(m_menuPopup);
    m_updateLabel->setProperty("class", QStringLiteral("AvarExtensionUpdate"));
    m_updateLabel->setWordWrap(true);
    m_updateLabel->hide();
    root->addWidget(m_updateLabel);

    updateMenuContent();
}

bool ExtensionIntegrationButton::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_menuPopup && event->type() == QEvent::Hide) {
        m_menuOpen = false;
    }
    return QWidget::eventFilter(watched, event);
}

void ExtensionIntegrationButton::toggleMenu()
{
    if (m_menuPopup->isVisible()) {
        closeMenu();
        return;
    }
    refreshStatus();
    updateMenuContent();
    positionMenu();
    m_menuPopup->show();
    m_menuOpen = true;
}

void ExtensionIntegrationButton::closeMenu()
{
    if (m_menuPopup != nullptr && m_menuPopup->isVisible()) {
        m_menuPopup->hide();
    }
    m_menuOpen = false;
}

void ExtensionIntegrationButton::positionMenu()
{
    if (m_menuPopup == nullptr || m_triggerHost == nullptr) {
        return;
    }
    const int menuWidth = 320;
    m_menuPopup->setFixedWidth(menuWidth);
    m_menuPopup->adjustSize();

    const QPoint bottomRight =
        m_triggerHost->mapToGlobal(QPoint(m_triggerHost->width(), m_triggerHost->height()));
    const int x = bottomRight.x() - m_menuPopup->width();
    const int y = bottomRight.y() + 6;
    m_menuPopup->move(x, y);
}

void ExtensionIntegrationButton::refreshStatus()
{
    m_checking = true;
    updateAppearance();
    updateMenuContent();

    m_bridge.requestStatus([this](const ExtensionBridgeStatus &status) {
        m_checking = false;
        m_bridgeReachable = status.bridgeReachable;
        m_extensionConnected = status.extensionConnected;
        m_bridgeVersion = status.bridgeVersion;
        m_protocolVersion = status.protocolVersion;
        m_extensionVersion = status.extensionVersion;
        updateAppearance();
        updateMenuContent();
    });
}

void ExtensionIntegrationButton::updateAppearance()
{
    const bool enabled = m_appSettings.browserExtensionEnabled();
    const bool suspended = m_guiPreferences.extensionBridgeSuspended();

    QString statusText;
    QString dotState;
    if (!enabled) {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.disabled"));
        dotState = QStringLiteral("idle");
    } else if (suspended) {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.suspended"));
        dotState = QStringLiteral("idle");
    } else if (m_checking) {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.checking"));
        dotState = QStringLiteral("idle");
    } else if (m_bridgeReachable && m_extensionConnected) {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.connected"));
        dotState = QStringLiteral("ok");
    } else {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.disconnected"));
        dotState = QStringLiteral("error");
    }

    m_triggerHost->setToolTip(QStringLiteral("%1 — %2").arg(m_tr.tr(QStringLiteral("extensionPanel.aria")), statusText));
    m_dot->setProperty("state", dotState);
    m_dot->style()->unpolish(m_dot);
    m_dot->style()->polish(m_dot);

    if (m_trigger != nullptr) {
        const QColor iconColor = QColor(m_theme.currentTokens().textMuted);
        m_trigger->setIcon(FaIcon::solid(QStringLiteral("puzzle-piece"), 14, iconColor));
    }
}

void ExtensionIntegrationButton::updateMenuContent()
{
    if (m_menuPopup == nullptr) {
        return;
    }

    const bool enabled = m_appSettings.browserExtensionEnabled();
    const bool suspended = m_guiPreferences.extensionBridgeSuspended();

    QString statusText;
    QString dotState;
    if (!enabled) {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.disabled"));
        dotState = QStringLiteral("idle");
    } else if (suspended) {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.suspended"));
        dotState = QStringLiteral("idle");
    } else if (m_checking) {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.checking"));
        dotState = QStringLiteral("idle");
    } else if (m_bridgeReachable && m_extensionConnected) {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.connected"));
        dotState = QStringLiteral("ok");
    } else {
        statusText = m_tr.tr(QStringLiteral("extensionPanel.disconnected"));
        dotState = QStringLiteral("error");
    }

    m_menuStatusLabel->setText(statusText);
    m_menuStatusDot->setProperty("state", dotState);
    m_menuStatusDot->style()->unpolish(m_menuStatusDot);
    m_menuStatusDot->style()->polish(m_menuStatusDot);

    m_listenCheck->blockSignals(true);
    m_listenCheck->setChecked(enabled);
    m_listenCheck->blockSignals(false);

    m_suspendSection->setVisible(enabled);
    m_suspendBtn->setVisible(!suspended);
    m_resumeBtn->setVisible(suspended);
    m_suspendHint->setText(suspended ? m_tr.tr(QStringLiteral("extensionPanel.suspendedHint"))
                                     : m_tr.tr(QStringLiteral("extensionPanel.suspendHint")));

    m_bridgeUrlCode->setText(m_bridge.bridgeBaseUrl());
    m_bridgeVersionValue->setText(m_bridgeVersion);
    m_protocolVersionValue->setText(QStringLiteral("v%1").arg(m_protocolVersion));
    m_extensionVersionValue->setText(m_extensionVersion.isEmpty()
                                         ? m_tr.tr(QStringLiteral("extensionPanel.notConnected"))
                                         : m_extensionVersion);

    const bool updateAvailable =
        !m_extensionVersion.isEmpty()
        && m_extensionVersion != QString::fromUtf8(kBundledExtensionVersion);
    if (updateAvailable) {
        m_updateLabel->setText(m_tr.tr(QStringLiteral("extensionPanel.updateAvailable")));
        m_updateLabel->show();
    } else {
        m_updateLabel->hide();
    }
}

} // namespace avar::gui
