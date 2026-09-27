#pragma once

#include "api/DaemonTypes.hpp"

#include <QWidget>

class QLabel;

namespace avar::gui {

class Translator;

class FooterBar final : public QWidget {
    Q_OBJECT

public:
    FooterBar(Translator &translator, QWidget *parent = nullptr);

    void setHealth(const HealthInfo &health, bool valid);
    void setStats(const SystemStatsInfo &stats, bool valid);

signals:
    void consoleToggleRequested();

private:
    Translator &m_tr;
    QLabel *m_uptime = nullptr;
    QLabel *m_cpu = nullptr;
    QLabel *m_memory = nullptr;
    QLabel *m_network = nullptr;
};

} // namespace avar::gui
