#include "ui/widgets/FooterBar.hpp"

#include "i18n/Translator.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

namespace avar::gui {

namespace {
QString formatUptime(qint64 seconds)
{
    const qint64 h = seconds / 3600;
    const qint64 m = (seconds % 3600) / 60;
    const qint64 s = seconds % 60;
    if (h > 0) {
        return QStringLiteral("%1h %2m").arg(h).arg(m);
    }
    if (m > 0) {
        return QStringLiteral("%1m %2s").arg(m).arg(s);
    }
    return QStringLiteral("%1s").arg(s);
}
} // namespace

FooterBar::FooterBar(Translator &translator, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
{
    setObjectName(QStringLiteral("AvarFooter"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);

    m_uptime = new QLabel(QStringLiteral("—"), this);
    m_cpu = new QLabel(QStringLiteral("—"), this);
    m_memory = new QLabel(QStringLiteral("—"), this);
    m_network = new QLabel(QStringLiteral("—"), this);

    for (QLabel *label : {m_uptime, m_cpu, m_memory, m_network}) {
        label->setProperty("class", QStringLiteral("AvarFooterStat"));
        layout->addWidget(label);
    }
    layout->addStretch();

    m_consoleBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    m_consoleBtn->setText(m_tr.tr(QStringLiteral("console.toggle")));
    QObject::connect(m_consoleBtn, &QPushButton::clicked, this, &FooterBar::consoleToggleRequested);
    layout->addWidget(m_consoleBtn);
    applyConsoleButtonStyle();
}

void FooterBar::setConsoleButtonState(bool consoleOpen, bool hasUnseenErrors)
{
    m_consoleOpen = consoleOpen;
    m_hasUnseenErrors = hasUnseenErrors;
    applyConsoleButtonStyle();
}

void FooterBar::applyConsoleButtonStyle()
{
    if (!m_consoleBtn) {
        return;
    }
    AvarButtonVariant variant = AvarButtonVariant::Ghost;
    if (m_consoleOpen) {
        variant = AvarButtonVariant::Secondary;
    } else if (m_hasUnseenErrors) {
        variant = AvarButtonVariant::Danger;
    }
    QString className;
    switch (variant) {
    case AvarButtonVariant::Primary:
        className = QStringLiteral("AvarButtonPrimary");
        break;
    case AvarButtonVariant::Secondary:
        className = QStringLiteral("AvarButtonSecondary");
        break;
    case AvarButtonVariant::Ghost:
        className = QStringLiteral("AvarButtonGhost");
        break;
    case AvarButtonVariant::Danger:
        className = QStringLiteral("AvarButtonDanger");
        break;
    }
    m_consoleBtn->setProperty("class", className);
    m_consoleBtn->style()->unpolish(m_consoleBtn);
    m_consoleBtn->style()->polish(m_consoleBtn);
}

void FooterBar::setHealth(const HealthInfo &health, bool valid)
{
    m_uptime->setText(valid ? QStringLiteral("%1: %2")
                                  .arg(m_tr.tr(QStringLiteral("health.uptime")),
                                       formatUptime(health.uptimeSeconds))
                            : QStringLiteral("—"));
}

void FooterBar::setStats(const SystemStatsInfo &stats, bool valid)
{
    if (!valid) {
        m_cpu->setText(QStringLiteral("—"));
        m_memory->setText(QStringLiteral("—"));
        m_network->setText(QStringLiteral("—"));
        return;
    }
    m_cpu->setText(QStringLiteral("CPU: %1%").arg(QString::number(stats.cpuUsagePercent, 'f', 1)));
    m_memory->setText(QStringLiteral("RAM: %1%").arg(QString::number(stats.memoryUsedPercent, 'f', 1)));
    m_network->setText(QStringLiteral("NET: ↓%1 ↑%2")
                           .arg(stats.networkRxBytesPerSec)
                           .arg(stats.networkTxBytesPerSec));
}

} // namespace avar::gui
