#pragma once

#include "api/DaemonClient.hpp"

#include <QWidget>

class QTextEdit;

namespace avar::gui {

class Translator;

class BatchAddDownloadsPopupPage final : public QWidget {
    Q_OBJECT

public:
    BatchAddDownloadsPopupPage(Translator &translator, DaemonClient &daemon, QWidget *parent = nullptr);

    void setDefaultQueue(const QString &queueId);

signals:
    void accepted(int count);
    void cancelled();

private:
    Translator &m_tr;
    DaemonClient &m_daemon;
    QTextEdit *m_urls = nullptr;
    QString m_queueId;
};

} // namespace avar::gui
