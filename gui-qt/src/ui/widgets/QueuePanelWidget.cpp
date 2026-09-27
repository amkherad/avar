#include "ui/widgets/QueuePanelWidget.hpp"

#include "i18n/Translator.hpp"
#include "theme/FaIcon.hpp"
#include "theme/ThemeManager.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

namespace avar::gui {

QueuePanelWidget::QueuePanelWidget(Translator &translator, ThemeManager &theme, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_theme(theme)
{
    setObjectName(QStringLiteral("AvarQueuePanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QHBoxLayout();
    auto *title = new QLabel(m_tr.tr(QStringLiteral("queue.title")), this);
    title->setProperty("class", QStringLiteral("AvarSidebarNavTitle"));
    title->setObjectName(QStringLiteral("AvarQueuePanelTitle"));
    header->addWidget(title);
    header->addStretch();

    auto *addBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    addBtn->setText(QStringLiteral("+"));
    addBtn->setObjectName(QStringLiteral("AvarQueueAddBtn"));
    addBtn->setToolTip(m_tr.tr(QStringLiteral("queue.add")));
    connect(addBtn, &QPushButton::clicked, this, &QueuePanelWidget::addQueueRequested);

    auto *settingsBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    settingsBtn->setObjectName(QStringLiteral("AvarQueueSettingsBtn"));
    settingsBtn->setIcon(FaIcon::solid(QStringLiteral("gear"), 14, QColor(m_theme.currentTokens().textMuted)));
    settingsBtn->setIconSize(QSize(14, 14));
    settingsBtn->setToolTip(m_tr.tr(QStringLiteral("nav.settings")));
    connect(settingsBtn, &QPushButton::clicked, this, &QueuePanelWidget::openQueueSettingsRequested);

    header->addWidget(addBtn);
    header->addWidget(settingsBtn);
    layout->addLayout(header);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("AvarQueueList"));
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) {
            return;
        }
        const QString id = m_list->item(row)->data(Qt::UserRole).toString();
        emit queueSelected(id);
    });
    layout->addWidget(m_list, 1);

    connect(&m_theme, &ThemeManager::themeChanged, this, [this] { refreshIcons(); });
}

void QueuePanelWidget::setQueues(const QVector<QueueInfo> &queues)
{
    const QString previous = selectedQueueId();
    m_list->clear();
    for (const QueueInfo &queue : queues) {
        auto *item = new QListWidgetItem(queue.name);
        item->setData(Qt::UserRole, queue.id);
        m_list->addItem(item);
    }
    if (!previous.isEmpty()) {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(Qt::UserRole).toString() == previous) {
                m_list->setCurrentRow(i);
                return;
            }
        }
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
}

QString QueuePanelWidget::selectedQueueId() const
{
    if (!m_list || m_list->currentRow() < 0) {
        return {};
    }
    return m_list->currentItem()->data(Qt::UserRole).toString();
}

void QueuePanelWidget::retranslateUi()
{
    if (auto *title = findChild<QLabel *>(QStringLiteral("AvarQueuePanelTitle"))) {
        title->setText(m_tr.tr(QStringLiteral("queue.title")));
    }
    if (auto *addBtn = findChild<QPushButton *>(QStringLiteral("AvarQueueAddBtn"))) {
        addBtn->setToolTip(m_tr.tr(QStringLiteral("queue.add")));
    }
    if (auto *settingsBtn = findChild<QPushButton *>(QStringLiteral("AvarQueueSettingsBtn"))) {
        settingsBtn->setToolTip(m_tr.tr(QStringLiteral("nav.settings")));
    }
    refreshIcons();
}

void QueuePanelWidget::refreshIcons()
{
    if (auto *settingsBtn = findChild<QPushButton *>(QStringLiteral("AvarQueueSettingsBtn"))) {
        settingsBtn->setIcon(
            FaIcon::solid(QStringLiteral("gear"), 14, QColor(m_theme.currentTokens().textMuted)));
    }
}

} // namespace avar::gui
