#include "session/SessionManager.hpp"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace avar::gui {

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    load();
    if (m_sessions.isEmpty()) {
        SessionRecord local;
        local.id = QStringLiteral("local");
        local.label = QStringLiteral("Local");
        local.baseUrl = QStringLiteral("http://127.0.0.1:8000");
        local.builtin = true;
        m_sessions.push_back(local);
        m_activeId = local.id;
        save();
    }
}

QVector<SessionRecord> SessionManager::sessions() const
{
    return m_sessions;
}

SessionRecord SessionManager::activeSession() const
{
    for (const SessionRecord &session : m_sessions) {
        if (session.id == m_activeId) {
            return session;
        }
    }
    return m_sessions.front();
}

QString SessionManager::activeSessionId() const
{
    return m_activeId;
}

void SessionManager::setActiveSessionId(const QString &id)
{
    if (m_activeId == id) {
        return;
    }
    m_activeId = id;
    save();
    emit sessionsChanged();
    emit activeSessionChanged(activeSession());
}

void SessionManager::upsertSession(const SessionRecord &session)
{
    for (SessionRecord &existing : m_sessions) {
        if (existing.id == session.id) {
            existing = session;
            save();
            emit sessionsChanged();
            return;
        }
    }
    m_sessions.push_back(session);
    save();
    emit sessionsChanged();
}

void SessionManager::removeSession(const QString &id)
{
    if (m_sessions.size() <= 1) {
        return;
    }
    m_sessions.erase(std::remove_if(m_sessions.begin(),
                                    m_sessions.end(),
                                    [&](const SessionRecord &s) { return s.id == id; }),
                     m_sessions.end());
    if (m_activeId == id) {
        m_activeId = m_sessions.front().id;
        emit activeSessionChanged(activeSession());
    }
    save();
    emit sessionsChanged();
}

void SessionManager::load()
{
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    m_activeId = store.value(QStringLiteral("sessions/activeId")).toString();
    const QByteArray raw = store.value(QStringLiteral("sessions/data")).toByteArray();
    m_sessions.clear();
    if (!raw.isEmpty()) {
        const QJsonArray arr = QJsonDocument::fromJson(raw).array();
        for (const QJsonValue &value : arr) {
            const QJsonObject obj = value.toObject();
            SessionRecord session;
            session.id = obj.value(QStringLiteral("id")).toString();
            session.label = obj.value(QStringLiteral("label")).toString();
            session.baseUrl = obj.value(QStringLiteral("baseUrl")).toString();
            session.authToken = obj.value(QStringLiteral("authToken")).toString();
            session.builtin = obj.value(QStringLiteral("builtin")).toBool();
            m_sessions.push_back(session);
        }
    }
}

void SessionManager::save()
{
    QJsonArray arr;
    for (const SessionRecord &session : m_sessions) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), session.id);
        obj.insert(QStringLiteral("label"), session.label);
        obj.insert(QStringLiteral("baseUrl"), session.baseUrl);
        obj.insert(QStringLiteral("authToken"), session.authToken);
        obj.insert(QStringLiteral("builtin"), session.builtin);
        arr.append(obj);
    }
    QSettings store(QStringLiteral("Avar"), QStringLiteral("gui-qt"));
    store.setValue(QStringLiteral("sessions/data"), QJsonDocument(arr).toJson(QJsonDocument::Compact));
    store.setValue(QStringLiteral("sessions/activeId"), m_activeId);
}

} // namespace avar::gui
