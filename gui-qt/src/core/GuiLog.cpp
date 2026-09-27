#include "core/GuiLog.hpp"

#include <QDateTime>

namespace avar::gui {

GuiLog &GuiLog::instance()
{
    static GuiLog log;
    return log;
}

void GuiLog::info(const QString &message)
{
    const QString line =
        QStringLiteral("[%1] [gui] [info] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate), message);
    emit lineAppended(line);
}

void GuiLog::warn(const QString &message)
{
    const QString line =
        QStringLiteral("[%1] [gui] [warn] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate), message);
    emit lineAppended(line);
}

void GuiLog::error(const QString &message)
{
    const QString line =
        QStringLiteral("[%1] [gui] [error] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate), message);
    emit lineAppended(line);
}

} // namespace avar::gui
