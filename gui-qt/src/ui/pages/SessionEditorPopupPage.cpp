#include "ui/pages/SessionEditorPopupPage.hpp"

#include "api/DaemonClient.hpp"
#include "i18n/Translator.hpp"
#include "session/SessionManager.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#include <QVBoxLayout>

namespace avar::gui {

namespace {

QString newSessionId()
{
    return QStringLiteral("session-%1").arg(QDateTime::currentMSecsSinceEpoch(), 0, 16);
}

} // namespace

SessionEditorPopupPage::SessionEditorPopupPage(Translator &translator,
                                               SessionManager &sessions,
                                               DaemonClient &daemon,
                                               QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_sessions(sessions)
    , m_daemon(daemon)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *form = new QFormLayout();
    m_label = new QLineEdit(this);
    form->addRow(m_tr.tr(QStringLiteral("session.label")), m_label);

    m_baseUrl = new QLineEdit(QStringLiteral("http://127.0.0.1:8000"), this);
    form->addRow(m_tr.tr(QStringLiteral("session.baseUrl")), m_baseUrl);

    m_authToken = new QLineEdit(this);
    m_authToken->setEchoMode(QLineEdit::Password);
    m_authToken->setClearButtonEnabled(true);
    form->addRow(m_tr.tr(QStringLiteral("session.authToken")), m_authToken);

    layout->addLayout(form);

    m_testStatus = new QLabel(this);
    m_testStatus->setProperty("class", QStringLiteral("AvarSettingsHint"));
    m_testStatus->hide();
    layout->addWidget(m_testStatus);

    auto *actions = new QHBoxLayout();
    auto *cancelBtn = new AvarButton(AvarButtonVariant::Ghost, this);
    cancelBtn->setText(m_tr.tr(QStringLiteral("dialog.cancel")));
    connect(cancelBtn, &QPushButton::clicked, this, &SessionEditorPopupPage::cancelled);

    auto *testBtn = new AvarButton(AvarButtonVariant::Secondary, this);
    testBtn->setText(m_tr.tr(QStringLiteral("session.test")));
    connect(testBtn, &QPushButton::clicked, this, &SessionEditorPopupPage::runConnectionTest);

    auto *saveBtn = new AvarButton(AvarButtonVariant::Primary, this);
    saveBtn->setText(m_tr.tr(QStringLiteral("common.save")));
    connect(saveBtn, &QPushButton::clicked, this, &SessionEditorPopupPage::saveSession);

    actions->addStretch();
    actions->addWidget(cancelBtn);
    actions->addWidget(testBtn);
    actions->addWidget(saveBtn);
    layout->addLayout(actions);
}

void SessionEditorPopupPage::setEditingSessionId(const QString &id)
{
    m_editingId = id;
    if (!id.isEmpty()) {
        loadFromSession(id);
    }
}

void SessionEditorPopupPage::loadFromSession(const QString &id)
{
    for (const SessionRecord &session : m_sessions.sessions()) {
        if (session.id != id) {
            continue;
        }
        m_label->setText(session.label);
        m_baseUrl->setText(session.baseUrl);
        m_authToken->setText(session.authToken);
        return;
    }
}

void SessionEditorPopupPage::runConnectionTest()
{
    m_testStatus->show();
    m_testStatus->setText(m_tr.tr(QStringLiteral("session.connecting")));
    m_testStatus->setProperty("status", QStringLiteral("pending"));
    m_testStatus->style()->unpolish(m_testStatus);
    m_testStatus->style()->polish(m_testStatus);

    DaemonClient::Options opts;
    opts.baseUrl = m_baseUrl->text().trimmed();
    opts.authToken = m_authToken->text().trimmed();
    auto *probe = new DaemonClient(opts, this);

    probe->ping(false, [this, probe](bool ok) {
        m_testStatus->setText(ok ? m_tr.tr(QStringLiteral("session.connected"))
                                 : m_tr.tr(QStringLiteral("session.disconnected")));
        m_testStatus->setProperty("status", ok ? QStringLiteral("ok") : QStringLiteral("fail"));
        m_testStatus->style()->unpolish(m_testStatus);
        m_testStatus->style()->polish(m_testStatus);
        probe->deleteLater();
    });
}

void SessionEditorPopupPage::saveSession()
{
    const QString id = m_editingId.isEmpty() ? newSessionId() : m_editingId;

    SessionRecord session;
    session.id = id;
    session.label = m_label->text().trimmed();
    if (session.label.isEmpty()) {
        session.label = QStringLiteral("Session");
    }
    session.baseUrl = m_baseUrl->text().trimmed();
    session.authToken = m_authToken->text().trimmed();
    session.builtin = false;

    m_sessions.upsertSession(session);
    if (m_editingId.isEmpty()) {
        m_sessions.setActiveSessionId(id);
    }
    emit saved();
}

} // namespace avar::gui
