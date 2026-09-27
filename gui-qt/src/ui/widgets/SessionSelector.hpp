#pragma once

#include "sync/SyncCoordinator.hpp"

#include <QWidget>

class QMenu;
class QLabel;
class QAbstractButton;
class QPushButton;

namespace avar::gui {

class Translator;
class SessionManager;

class SessionSelector final : public QWidget {
    Q_OBJECT

public:
    SessionSelector(Translator &translator, SessionManager &sessions, QWidget *parent = nullptr);

    void setConnectionState(ConnectionState state);

signals:
    void refreshRequested();

private:
    void rebuildMenu();
    void updateLabels();

    Translator &m_tr;
    SessionManager &m_sessions;
    QAbstractButton *m_trigger = nullptr;
    QPushButton *m_refresh = nullptr;
    QLabel *m_status = nullptr;
    QMenu *m_menu = nullptr;
    ConnectionState m_connection = ConnectionState::Disconnected;
};

} // namespace avar::gui
