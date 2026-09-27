#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace avar::gui {

class DaemonClient;
class SessionManager;
class Translator;

class SessionEditorPopupPage final : public QWidget {
    Q_OBJECT

public:
    SessionEditorPopupPage(Translator &translator,
                           SessionManager &sessions,
                           DaemonClient &daemon,
                           QWidget *parent = nullptr);

    void setEditingSessionId(const QString &id);

signals:
    void saved();
    void cancelled();

private:
    void loadFromSession(const QString &id);
    void runConnectionTest();
    void saveSession();

    Translator &m_tr;
    SessionManager &m_sessions;
    DaemonClient &m_daemon;

    QString m_editingId;
    QLineEdit *m_label = nullptr;
    QLineEdit *m_baseUrl = nullptr;
    QLineEdit *m_authToken = nullptr;
    QLabel *m_testStatus = nullptr;
};

} // namespace avar::gui
