#include "ui/pages/ConfirmDialogPopupPage.hpp"

#include "i18n/Translator.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace avar::gui {

ConfirmDialogPopupPage::ConfirmDialogPopupPage(Translator &translator, QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *label = new QLabel(this);
    label->setObjectName(QStringLiteral("ConfirmMessage"));
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *actions = new QHBoxLayout();
    auto *cancelBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    cancelBtn->setText(m_tr.tr(QStringLiteral("dialog.cancel")));
    connect(cancelBtn, &QPushButton::clicked, this, &ConfirmDialogPopupPage::cancelled);
    auto *okBtn = new AvarButton(AvarButtonVariant::Primary, this);
    okBtn->setText(m_tr.tr(QStringLiteral("dialog.confirm")));
    connect(okBtn, &QPushButton::clicked, this, &ConfirmDialogPopupPage::confirmed);
    actions->addStretch();
    actions->addWidget(cancelBtn);
    actions->addWidget(okBtn);
    layout->addLayout(actions);
}

void ConfirmDialogPopupPage::setMessage(const QString &message)
{
    if (auto *label = findChild<QLabel *>(QStringLiteral("ConfirmMessage"))) {
        label->setText(message);
    }
}

} // namespace avar::gui
