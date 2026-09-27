#include "ui/pages/AddDownloadPopupPage.hpp"

#include "i18n/Translator.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace avar::gui {

AddDownloadPopupPage::AddDownloadPopupPage(Translator &translator, DaemonClient &daemon, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_daemon(daemon)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);

    auto *form = new QFormLayout();
    m_url = new QLineEdit(this);
    m_url->setPlaceholderText(QStringLiteral("https://"));
    form->addRow(m_tr.tr(QStringLiteral("download.urlLabel")), m_url);
    layout->addLayout(form);

    auto *actions = new QHBoxLayout();
    auto *cancelBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    cancelBtn->setText(m_tr.tr(QStringLiteral("dialog.cancel")));
    connect(cancelBtn, &QPushButton::clicked, this, &AddDownloadPopupPage::cancelled);

    auto *okBtn = new AvarButton(AvarButtonVariant::Primary, this);
    okBtn->setText(m_tr.tr(QStringLiteral("dialog.add")));
    connect(okBtn, &QPushButton::clicked, this, [this] {
        const QString url = m_url->text().trimmed();
        if (!url.isEmpty()) {
            m_daemon.addDownload(url, m_queueId);
            emit accepted(url);
        }
    });
    actions->addStretch();
    actions->addWidget(cancelBtn);
    actions->addWidget(okBtn);
    layout->addLayout(actions);
}

void AddDownloadPopupPage::setDefaultQueue(const QString &queueId)
{
    m_queueId = queueId;
}

} // namespace avar::gui
