#include "ui/widgets/WindowTitleBar.hpp"

#include "ui/widgets/AvarButton.hpp"

#include <QHBoxLayout>
#include <QLabel>

namespace avar::gui {

WindowTitleBar::WindowTitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("AvarWindowTitleBar"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    m_title = new QLabel(QStringLiteral("Avar"), this);
    m_title->setObjectName(QStringLiteral("AvarWindowTitle"));
    layout->addWidget(m_title, 1);

    auto *minBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    minBtn->setText(QStringLiteral("—"));
    connect(minBtn, &QPushButton::clicked, this, &WindowTitleBar::minimizeRequested);

    auto *closeBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    closeBtn->setText(QStringLiteral("×"));
    connect(closeBtn, &QPushButton::clicked, this, &WindowTitleBar::closeRequested);

    layout->addWidget(minBtn);
    layout->addWidget(closeBtn);
}

void WindowTitleBar::setTitle(const QString &title)
{
    m_title->setText(title);
}

} // namespace avar::gui
