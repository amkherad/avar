#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace avar::gui {

struct SessionRecord {
    QString id;
    QString label;
    QString baseUrl;
    QString authToken;
    bool builtin = false;
};

class SessionManager final : public QObject {
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);

    [[nodiscard]] QVector<SessionRecord> sessions() const;
    [[nodiscard]] SessionRecord activeSession() const;
    [[nodiscard]] QString activeSessionId() const;

    void setActiveSessionId(const QString &id);
    void upsertSession(const SessionRecord &session);
    void removeSession(const QString &id);

    [[nodiscard]] static bool isBuiltinLocalSession(const SessionRecord &session);

signals:
    void sessionsChanged();
    void activeSessionChanged(const SessionRecord &session);

private:
    void load();
    void save();

    QVector<SessionRecord> m_sessions;
    QString m_activeId;
};

} // namespace avar::gui
