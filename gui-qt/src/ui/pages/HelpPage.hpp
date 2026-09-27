#pragma once

#include <QWidget>

namespace avar::gui {

class Translator;

class HelpPage final : public QWidget {
    Q_OBJECT

public:
    explicit HelpPage(Translator &translator, QWidget *parent = nullptr);
};

} // namespace avar::gui
