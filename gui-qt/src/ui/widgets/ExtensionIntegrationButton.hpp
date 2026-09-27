#pragma once

#include "extension/ExtensionBridgeClient.hpp"

#include <QWidget>

class QCheckBox;
class QFrame;
class QLabel;
class QPushButton;

namespace avar::gui {

class AppSettings;
class GuiPreferences;
class ThemeManager;
class Translator;

class ExtensionIntegrationButton final : public QWidget {
    Q_OBJECT

public:
    ExtensionIntegrationButton(Translator &translator,
                               ThemeManager &theme,
                               ExtensionBridgeClient &bridge,
                               AppSettings &appSettings,
                               GuiPreferences &guiPreferences,
                               QWidget *parent = nullptr);

    void retranslateUi();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildMenu();
    void toggleMenu();
    void closeMenu();
    void positionMenu();
    void refreshStatus();
    void updateAppearance();
    void updateMenuContent();

    Translator &m_tr;
    ThemeManager &m_theme;
    ExtensionBridgeClient &m_bridge;
    AppSettings &m_appSettings;
    GuiPreferences &m_guiPreferences;

    QWidget *m_triggerHost = nullptr;
    QPushButton *m_trigger = nullptr;
    QLabel *m_dot = nullptr;

    QFrame *m_menuPopup = nullptr;
    QLabel *m_menuStatusDot = nullptr;
    QLabel *m_menuStatusLabel = nullptr;
    QCheckBox *m_listenCheck = nullptr;
    QWidget *m_suspendSection = nullptr;
    QPushButton *m_suspendBtn = nullptr;
    QPushButton *m_resumeBtn = nullptr;
    QLabel *m_suspendHint = nullptr;
    QLabel *m_bridgeUrlCode = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QLabel *m_bridgeVersionValue = nullptr;
    QLabel *m_protocolVersionValue = nullptr;
    QLabel *m_extensionVersionValue = nullptr;
    QLabel *m_bundledVersionValue = nullptr;
    QLabel *m_updateLabel = nullptr;

    bool m_menuOpen = false;
    bool m_bridgeReachable = false;
    bool m_extensionConnected = false;
    bool m_checking = true;
    QString m_bridgeVersion = QStringLiteral("0.1.0");
    int m_protocolVersion = 1;
    QString m_extensionVersion;
};

} // namespace avar::gui
