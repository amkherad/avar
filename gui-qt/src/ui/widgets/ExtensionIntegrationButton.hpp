#pragma once

#include <QWidget>

class QLabel;

namespace avar::gui {

class ExtensionBridgeClient;
class Translator;

class ExtensionIntegrationButton final : public QWidget {
    Q_OBJECT

public:
    ExtensionIntegrationButton(Translator &translator, ExtensionBridgeClient &bridge, QWidget *parent = nullptr);

private:
    void refreshStatus();

    Translator &m_tr;
    ExtensionBridgeClient &m_bridge;
    QLabel *m_status = nullptr;
};

} // namespace avar::gui
