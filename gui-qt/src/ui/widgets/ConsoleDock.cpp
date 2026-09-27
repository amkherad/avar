#include "ui/widgets/ConsoleDock.hpp"

#include "config/LayoutPreferences.hpp"
#include "i18n/Translator.hpp"
#include "ui/widgets/AvarButton.hpp"
#include "ui/widgets/ResizeHandle.hpp"

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace avar::gui {

ConsoleDock::ConsoleDock(Translator &translator, LayoutPreferences &layout, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("AvarConsole"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *resize = new ResizeHandle(ResizeAxis::Vertical, this);
    connect(resize, &ResizeHandle::resizeDelta, &layout, &LayoutPreferences::adjustConsoleHeight);

    auto *header = new QHBoxLayout();
    auto *title = new QLabel(translator.tr(QStringLiteral("console.title")), this);
    title->setProperty("class", QStringLiteral("AvarConsoleTitle"));
    auto *clearBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    clearBtn->setText(translator.tr(QStringLiteral("console.clear")));
    header->addWidget(title);
    header->addStretch();
    header->addWidget(clearBtn);

    m_output = new QTextEdit(this);
    m_output->setObjectName(QStringLiteral("AvarConsoleOutput"));
    m_output->setReadOnly(true);

    connect(clearBtn, &QPushButton::clicked, m_output, &QTextEdit::clear);

    outer->addWidget(resize);
    auto *headerWidget = new QWidget(this);
    headerWidget->setLayout(header);
    outer->addWidget(headerWidget);
    outer->addWidget(m_output, 1);

    setOpen(layout.consoleOpen());
    connect(&layout, &LayoutPreferences::layoutChanged, this, [this, &layout] { setOpen(layout.consoleOpen()); });
}

void ConsoleDock::appendLine(const QString &line)
{
    m_output->append(line);
}

void ConsoleDock::setOpen(bool open)
{
    setVisible(open);
}

} // namespace avar::gui
