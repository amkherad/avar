#include "ui/pages/SettingsPage.hpp"

#include "api/DaemonClient.hpp"
#include "config/AppSettings.hpp"
#include "i18n/Translator.hpp"

#include <QJsonObject>

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace avar::gui {

SettingsPage::SettingsPage(Translator &translator,
                           AppSettings &settings,
                           DaemonClient &daemon,
                           QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_settings(settings)
    , m_daemon(daemon)
{
    m_stack = new QStackedWidget(this);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_stack);

    m_stack->addWidget(makeGeneralPanel());
    m_stack->addWidget(makeDownloadsPanel());
    m_stack->addWidget(makeQueuesPanel());
    m_stack->addWidget(makeDaemonPanel());
    m_stack->addWidget(makeBrowserPanel());
    m_stack->addWidget(makeShortcutsPanel());
    m_stack->addWidget(makeAboutPanel());

    setCategory(SettingsCategory::General);
}

void SettingsPage::setCategory(SettingsCategory category)
{
    m_stack->setCurrentIndex(static_cast<int>(category));
}

QWidget *SettingsPage::makeGeneralPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QFormLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *title = new QLabel(m_tr.tr(QStringLiteral("settings.categories.general")), panel);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px;"));
    layout->addRow(title);

    auto *locale = new QLineEdit(m_settings.locale(), panel);
    connect(locale, &QLineEdit::editingFinished, this, [this, locale] {
        m_settings.setLocale(locale->text());
    });
    layout->addRow(m_tr.tr(QStringLiteral("settings.general.locale")), locale);

    return panel;
}

QWidget *SettingsPage::makeDownloadsPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(m_tr.tr(QStringLiteral("settings.categories.downloads")), panel);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px;"));
    layout->addWidget(title);

    auto *hint = new QLabel(m_tr.tr(QStringLiteral("settings.downloads.hint")), panel);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *pathLabel = new QLabel(m_tr.tr(QStringLiteral("settings.downloads.defaultPath")), panel);
    layout->addWidget(pathLabel);
    auto *pathValue = new QLabel(panel);
    pathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(pathValue);
    m_daemon.getConfig(QStringLiteral("download.defaultPath"), {}, [pathValue](const QString &value) {
        pathValue->setText(value.isEmpty() ? QStringLiteral("—") : value);
    });

    layout->addStretch();
    return panel;
}

QWidget *SettingsPage::makeQueuesPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(m_tr.tr(QStringLiteral("settings.categories.queues")), panel);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px;"));
    layout->addWidget(title);
    auto *hint = new QLabel(m_tr.tr(QStringLiteral("settings.queues.hint")), panel);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch();
    return panel;
}

QWidget *SettingsPage::makeDaemonPanel()
{
    auto *panel = new QWidget(this);
    auto *form = new QFormLayout(panel);
    form->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(m_tr.tr(QStringLiteral("settings.categories.daemon")), panel);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px;"));
    form->addRow(title);

    auto *baseUrl = new QLineEdit(m_settings.daemonBaseUrl(), panel);
    connect(baseUrl, &QLineEdit::editingFinished, this, [this, baseUrl] {
        m_settings.setDaemonBaseUrl(baseUrl->text());
    });
    form->addRow(m_tr.tr(QStringLiteral("session.baseUrl")), baseUrl);

    auto *token = new QLineEdit(m_settings.authToken(), panel);
    token->setEchoMode(QLineEdit::Password);
    connect(token, &QLineEdit::editingFinished, this, [this, token] {
        m_settings.setAuthToken(token->text());
    });
    form->addRow(m_tr.tr(QStringLiteral("session.authToken")), token);

    return panel;
}

QWidget *SettingsPage::makeBrowserPanel()
{
    auto *panel = new QWidget(this);
    auto *form = new QFormLayout(panel);
    form->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(m_tr.tr(QStringLiteral("settings.categories.browser")), panel);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px;"));
    form->addRow(title);

    auto *extension = new QCheckBox(m_tr.tr(QStringLiteral("settings.browser.enableExtension")), panel);
    extension->setChecked(m_settings.browserExtensionEnabled());
    connect(extension, &QCheckBox::toggled, this, [this](bool checked) {
        m_settings.setBrowserExtensionEnabled(checked);
    });
    form->addRow(extension);

    return panel;
}

QWidget *SettingsPage::makeShortcutsPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(m_tr.tr(QStringLiteral("settings.categories.shortcuts")), panel);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px;"));
    layout->addWidget(title);
    layout->addWidget(new QLabel(m_tr.tr(QStringLiteral("settings.shortcuts.hint")), panel));
    layout->addStretch();
    return panel;
}

QWidget *SettingsPage::makeAboutPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *title = new QLabel(m_tr.tr(QStringLiteral("settings.categories.about")), panel);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px;"));
    layout->addWidget(title);

    layout->addWidget(new QLabel(QStringLiteral("Avar GUI Qt %1").arg(QStringLiteral("0.1.0")), panel));

    auto *backend = new QLabel(m_tr.tr(QStringLiteral("settings.about.backendLoading")), panel);
    backend->setObjectName(QStringLiteral("AboutBackendVersion"));
    layout->addWidget(backend);

    m_daemon.cliExec({QStringLiteral("avar"), QStringLiteral("--version")}, [backend, this](bool ok, const QJsonObject &result, const QString &) {
        if (!ok) {
            backend->setText(m_tr.tr(QStringLiteral("settings.about.backendUnknown")));
            return;
        }
        const QString output = result.value(QStringLiteral("output")).toString().trimmed();
        backend->setText(output.isEmpty() ? m_tr.tr(QStringLiteral("settings.about.backendUnknown")) : output);
    });

    layout->addStretch();
    return panel;
}

} // namespace avar::gui
