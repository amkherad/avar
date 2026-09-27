#include "ui/pages/DownloadDetailPopupPage.hpp"

#include "i18n/Translator.hpp"

#include <QLabel>
#include <QVBoxLayout>

namespace avar::gui {

DownloadDetailPopupPage::DownloadDetailPopupPage(Translator &translator, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->addWidget(new QLabel(m_tr.tr(QStringLiteral("download.detailsTitle")), this));
}

void DownloadDetailPopupPage::setDownload(const DownloadInfo &info)
{
    if (auto *body = findChild<QLabel *>(QStringLiteral("DetailBody"))) {
        body->setText(QStringLiteral("%1\n%2\n%3").arg(info.name, info.status, info.id));
        return;
    }
    auto *body = new QLabel(this);
    body->setObjectName(QStringLiteral("DetailBody"));
    body->setWordWrap(true);
    body->setText(QStringLiteral("%1\n%2\n%3").arg(info.name, info.status, info.id));
    layout()->addWidget(body);
}

} // namespace avar::gui
