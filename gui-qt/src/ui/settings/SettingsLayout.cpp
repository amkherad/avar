#include "ui/settings/SettingsLayout.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>

namespace avar::gui {

QWidget *wrapSettingsPage(QWidget *content, int maxContentWidth)
{
    auto *form = new QWidget();
    form->setObjectName(QStringLiteral("AvarSettingsForm"));
    if (maxContentWidth > kSettingsFormMaxWidth) {
        form->setProperty("wide", true);
    }
    form->setMaximumWidth(maxContentWidth);
    form->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(0);
    formLayout->addWidget(content);

    auto *host = new QWidget();
    auto *hostLayout = new QHBoxLayout(host);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);
    hostLayout->addWidget(form, 0, Qt::AlignTop);
    hostLayout->addStretch(1);

    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(host);
    return scroll;
}

} // namespace avar::gui
