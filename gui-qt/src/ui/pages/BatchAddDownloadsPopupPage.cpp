#include "ui/pages/BatchAddDownloadsPopupPage.hpp"

#include "i18n/Translator.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace avar::gui {

BatchAddDownloadsPopupPage::BatchAddDownloadsPopupPage(Translator &translator,
                                                       DaemonClient &daemon,
                                                       QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_daemon(daemon)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(m_tr.tr(QStringLiteral("download.batchAdd.button")), this);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px;"));
    layout->addWidget(title);

    m_urls = new QTextEdit(this);
    m_urls->setPlaceholderText(m_tr.tr(QStringLiteral("download.batchAdd.placeholder")));
    layout->addWidget(m_urls, 1);

    auto *actions = new QHBoxLayout();
    auto *cancelBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    cancelBtn->setText(m_tr.tr(QStringLiteral("dialog.cancel")));
    connect(cancelBtn, &QPushButton::clicked, this, &BatchAddDownloadsPopupPage::cancelled);

    auto *okBtn = new AvarButton(AvarButtonVariant::Primary, this);
    okBtn->setText(m_tr.tr(QStringLiteral("dialog.add")));
    connect(okBtn, &QPushButton::clicked, this, [this] {
        int count = 0;
        for (const QString &line : m_urls->toPlainText().split(QLatin1Char('\n'))) {
            const QString url = line.trimmed();
            if (url.isEmpty() || url.startsWith(QLatin1Char('#'))) {
                continue;
            }
            m_daemon.addDownload(url, m_queueId);
            ++count;
        }
        emit accepted(count);
    });
    actions->addStretch();
    actions->addWidget(cancelBtn);
    actions->addWidget(okBtn);
    layout->addLayout(actions);
}

void BatchAddDownloadsPopupPage::setDefaultQueue(const QString &queueId)
{
    m_queueId = queueId;
}

} // namespace avar::gui
