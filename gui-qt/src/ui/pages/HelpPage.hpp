#pragma once

#include <QWidget>

class QTextBrowser;

namespace avar::gui {

class AppSettings;
class Translator;

class HelpPage final : public QWidget {
    Q_OBJECT

public:
    HelpPage(Translator &translator, AppSettings &settings, QWidget *parent = nullptr);

    void setTopicId(const QString &id);
    [[nodiscard]] QString topicId() const;

public slots:
    void reloadContent();

private:
    void applyContent();

    Translator &m_tr;
    AppSettings &m_settings;
    QString m_topicId;
    QTextBrowser *m_browser = nullptr;
};

} // namespace avar::gui
