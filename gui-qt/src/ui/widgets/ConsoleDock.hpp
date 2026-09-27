#pragma once

#include <QWidget>

class QTextEdit;

namespace avar::gui {

class Translator;
class LayoutPreferences;

class ConsoleDock final : public QWidget {
    Q_OBJECT

public:
    ConsoleDock(Translator &translator, LayoutPreferences &layout, QWidget *parent = nullptr);

    void appendLine(const QString &line);
    void setOpen(bool open);

private:
    QTextEdit *m_output = nullptr;
};

} // namespace avar::gui
