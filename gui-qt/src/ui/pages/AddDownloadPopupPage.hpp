#pragma once

#include "api/DaemonClient.hpp"

#include <QWidget>

class QLineEdit;

namespace avar::gui {

class Translator;

class AddDownloadPopupPage final : public QWidget {
    Q_OBJECT

public:
    AddDownloadPopupPage(Translator &translator, DaemonClient &daemon, QWidget *parent = nullptr);

    void setDefaultQueue(const QString &queueId);

signals:
    void accepted(const QString &url);
    void cancelled();

private:
    Translator &m_tr;
    DaemonClient &m_daemon;
    QLineEdit *m_url = nullptr;
    QString m_queueId;
};

} // namespace avar::gui
