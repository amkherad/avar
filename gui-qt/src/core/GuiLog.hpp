#pragma once

#include <QObject>
#include <QString>

namespace avar::gui {

class GuiLog final : public QObject {
    Q_OBJECT

public:
    static GuiLog &instance();

    void info(const QString &message);
    void warn(const QString &message);
    void error(const QString &message);

signals:
    void lineAppended(const QString &line);

private:
    GuiLog() = default;
};

} // namespace avar::gui
