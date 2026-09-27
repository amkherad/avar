#pragma once

#include <QWidget>

namespace avar::gui {

class Translator;

class ConfirmDialogPopupPage final : public QWidget {
    Q_OBJECT

public:
    ConfirmDialogPopupPage(Translator &translator, QWidget *parent = nullptr);

    void setMessage(const QString &message);

signals:
    void confirmed();
    void cancelled();

private:
    Translator &m_tr;
};

} // namespace avar::gui
