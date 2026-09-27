#pragma once

#include <QVector>
#include <QWidget>

class QPushButton;

namespace avar::gui {

class Translator;

class HelpSidebarNav final : public QWidget {
    Q_OBJECT

public:
    explicit HelpSidebarNav(Translator &translator, QWidget *parent = nullptr);

    void setTopicId(const QString &id);
    void retranslateUi();

signals:
    void topicChanged(const QString &id);

private:
    Translator &m_tr;
    QVector<QPushButton *> m_buttons;
    QString m_topicId;
};

} // namespace avar::gui
