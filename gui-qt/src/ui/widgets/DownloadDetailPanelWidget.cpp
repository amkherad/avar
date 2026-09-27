#include "ui/widgets/DownloadDetailPanelWidget.hpp"

#include "i18n/Translator.hpp"

#include <QLabel>
#include <QVBoxLayout>

namespace avar::gui {

DownloadDetailPanelWidget::DownloadDetailPanelWidget(Translator &translator, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
{
    setObjectName(QStringLiteral("AvarDownloadPanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    m_title = new QLabel(m_tr.tr(QStringLiteral("download.detailsTitle")), this);
    m_title->setProperty("class", QStringLiteral("AvarDownloadPanelTitle"));
    m_body = new QLabel(this);
    m_body->setWordWrap(true);
    layout->addWidget(m_title);
    layout->addWidget(m_body, 1);
    setDownload({}, false);
}

void DownloadDetailPanelWidget::setDownload(const DownloadInfo &download, bool valid)
{
    m_download = download;
    m_downloadValid = valid;
    refreshBodyText();
}

void DownloadDetailPanelWidget::retranslateUi()
{
    if (m_title != nullptr) {
        m_title->setText(m_tr.tr(QStringLiteral("download.detailsTitle")));
    }
    refreshBodyText();
}

void DownloadDetailPanelWidget::refreshBodyText()
{
    if (m_body == nullptr) {
        return;
    }
    if (!m_downloadValid) {
        m_body->setText(m_tr.tr(QStringLiteral("download.selectHint")));
        return;
    }
    m_body->setText(QStringLiteral("%1\n\n%2\n%3\n%4")
                        .arg(m_download.name, m_download.status, m_download.id)
                        .arg(m_tr.tr(QStringLiteral("download.progress"))
                             + QStringLiteral(": %1%").arg(m_download.progress * 100.0, 0, 'f', 1)));
}

} // namespace avar::gui
