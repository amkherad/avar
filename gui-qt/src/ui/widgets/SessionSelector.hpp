#pragma once

#include "sync/SyncCoordinator.hpp"

#include <QWidget>

class QFrame;
class QResizeEvent;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace avar::gui {

class Translator;
class SessionManager;

class SessionSelector final : public QWidget {
    Q_OBJECT

public:
    SessionSelector(Translator &translator, SessionManager &sessions, QWidget *parent = nullptr);

    void setConnectionState(ConnectionState state);
    void retranslateUi();

signals:
    void refreshRequested();
    void addSessionRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void toggleMenu();
    void closeMenu();
    void rebuildMenu();
    void updateTrigger();
    void updateChevronIcon();
    void positionMenu();
    void setMenuOpen(bool open);
    void updateMenuScrollHeight();

    Translator &m_tr;
    SessionManager &m_sessions;

    QWidget *m_trigger = nullptr;
    QLabel *m_dot = nullptr;
    QLabel *m_label = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_chevron = nullptr;
    QPushButton *m_refresh = nullptr;

    QFrame *m_menuPopup = nullptr;
    QScrollArea *m_menuScroll = nullptr;
    QWidget *m_menuListHost = nullptr;
    QVBoxLayout *m_menuListLayout = nullptr;

    bool m_menuOpen = false;
    bool m_refreshing = false;
    ConnectionState m_connection = ConnectionState::Disconnected;
};

} // namespace avar::gui
