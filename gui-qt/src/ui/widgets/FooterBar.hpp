#pragma once

#include "api/DaemonTypes.hpp"

#include <QWidget>

class QLabel;

namespace avar::gui {

class AvarButton;
class Translator;

class FooterBar final : public QWidget {
    Q_OBJECT

public:
    FooterBar(Translator &translator, QWidget *parent = nullptr);

    void setHealth(const HealthInfo &health, bool valid);
    void setStats(const SystemStatsInfo &stats, bool valid);
    void setConsoleButtonState(bool consoleOpen, bool hasUnseenErrors);
    void retranslateUi();

signals:
    void consoleToggleRequested();

private:
    void applyConsoleButtonStyle();

    Translator &m_tr;
    AvarButton *m_consoleBtn = nullptr;
    bool m_consoleOpen = false;
    bool m_hasUnseenErrors = false;
    QLabel *m_uptime = nullptr;
    QLabel *m_cpu = nullptr;
    QLabel *m_memory = nullptr;
    QLabel *m_network = nullptr;
};

} // namespace avar::gui
