#include "ui/AppShell.hpp"

#include "config/LayoutPreferences.hpp"
#include "i18n/Translator.hpp"
#include "session/SessionManager.hpp"
#include "theme/ThemeManager.hpp"
#include "ui/widgets/AvarButton.hpp"
#include "ui/widgets/QueuePanelWidget.hpp"
#include "ui/widgets/ResizeHandle.hpp"
#include "ui/widgets/SessionSelector.hpp"
#include "ui/widgets/ExtensionIntegrationButton.hpp"
#include "ui/widgets/HeaderWindowDrag.hpp"
#include "ui/widgets/SettingsSidebarNav.hpp"
#include "ui/widgets/WindowControls.hpp"
#include "core/Hosting.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QStyle>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

namespace avar::gui {

AppShell::AppShell(Translator &translator,
                   ThemeManager &theme,
                   LayoutPreferences &layout,
                   SessionManager &sessions,
                   ExtensionBridgeClient &extensionBridge,
                   QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_theme(theme)
    , m_layout(layout)
    , m_sessions(sessions)
    , m_extension(extensionBridge)
{
    setObjectName(QStringLiteral("AvarRoot"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    buildHeader();
    outer->addWidget(m_header);

    auto *body = new QHBoxLayout();
    body->setSpacing(0);
    body->setContentsMargins(0, 0, 0, 0);

    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName(QStringLiteral("AvarSidebar"));
    m_sidebar->setFixedWidth(m_layout.sidebarWidth());
    auto *sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(16, 16, 16, 16);
    sidebarLayout->setSpacing(12);

    m_sidebarBody = new QStackedWidget(m_sidebar);

    m_queuePanel = new QueuePanelWidget(m_tr, m_sidebarBody);
    auto *dashboardSidebar = new QWidget(m_sidebarBody);
    auto *dashLayout = new QVBoxLayout(dashboardSidebar);
    dashLayout->setContentsMargins(0, 0, 0, 0);
    dashLayout->addWidget(m_queuePanel, 1);
    m_sessionSelector = new SessionSelector(m_tr, m_sessions, dashboardSidebar);
    dashLayout->addWidget(m_sessionSelector);

    m_settingsSidebarNav = new SettingsSidebarNav(m_tr, m_sidebarBody);
    m_helpSidebar = new QWidget(m_sidebarBody);
    auto *helpLayout = new QVBoxLayout(m_helpSidebar);
    helpLayout->addWidget(new QLabel(m_tr.tr(QStringLiteral("nav.help")), m_helpSidebar));

    m_sidebarBody->addWidget(dashboardSidebar);
    m_sidebarBody->addWidget(m_settingsSidebarNav);
    m_sidebarBody->addWidget(m_helpSidebar);
    sidebarLayout->addWidget(m_sidebarBody, 1);

    auto *sidebarResize = new ResizeHandle(ResizeAxis::Horizontal, this);
    connect(sidebarResize, &ResizeHandle::resizeDelta, &m_layout, &LayoutPreferences::adjustSidebarWidth);
    connect(&m_layout, &LayoutPreferences::layoutChanged, this, [this] {
        m_sidebar->setFixedWidth(m_layout.sidebarWidth());
    });

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("AvarPageStack"));

    body->addWidget(m_sidebar);
    body->addWidget(sidebarResize);
    body->addWidget(m_stack, 1);
    outer->addLayout(body, 1);

    connect(m_settingsSidebarNav, &SettingsSidebarNav::categoryChanged, this,
            &AppShell::openSettingsCategory);

    setPage(AppPage::Dashboard);
}

void AppShell::buildHeader()
{
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("AvarHeader"));
    if (detectHostingMode() == HostingMode::Desktop) {
        m_header->setProperty("desktop", true);
    }

    auto *grid = new QGridLayout(m_header);
    grid->setContentsMargins(8, 0, 0, 0);
    grid->setHorizontalSpacing(0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 0);
    grid->setColumnStretch(2, 1);

    m_backButton = new AvarButton(AvarButtonVariant::Ghost, m_header);
    m_backButton->setText(QStringLiteral("←"));
    connect(m_backButton, &QPushButton::clicked, this, [this] { setPage(AppPage::Dashboard); });

    auto *icon = new QLabel(m_header);
    icon->setObjectName(QStringLiteral("AvarHeaderIcon"));
    icon->setPixmap(QIcon(QStringLiteral(":/icon.svg")).pixmap(24, 24));

    auto *title = new QLabel(m_tr.tr(QStringLiteral("app.title")), m_header);
    title->setObjectName(QStringLiteral("AvarHeaderTitle"));
    auto *subtitle = new QLabel(m_tr.tr(QStringLiteral("app.subtitle")), m_header);
    subtitle->setObjectName(QStringLiteral("AvarHeaderSubtitle"));

    auto *brandHost = new QWidget(m_header);
    auto *brand = new QHBoxLayout(brandHost);
    brand->setContentsMargins(8, 0, 8, 0);
    brand->addWidget(m_backButton);
    brand->addWidget(icon);
    brand->addWidget(title);
    brand->addWidget(subtitle);
    brand->addStretch();

    auto *actionsHost = new QWidget(m_header);
    auto *actions = new QHBoxLayout(actionsHost);
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(4);
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    actions->addWidget(new ExtensionIntegrationButton(m_tr, m_extension, actionsHost));
    auto *separator = new QFrame(actionsHost);
    separator->setObjectName(QStringLiteral("AvarHeaderSeparator"));
    separator->setFrameShape(QFrame::VLine);
    actions->addWidget(separator);
#endif

    auto *themeBtn = new AvarButton(AvarButtonVariant::Ghost, actionsHost);
    themeBtn->setText(QStringLiteral("◐"));
    auto *themeMenu = new QMenu(themeBtn);
    const auto addThemeAction = [this, themeMenu](const QString &label, ThemeSetting setting) {
        auto *action = themeMenu->addAction(label);
        connect(action, &QAction::triggered, this, [this, setting] { emit themeSettingRequested(setting); });
    };
    addThemeAction(m_tr.tr(QStringLiteral("theme.lightSoft")), ThemeSetting::LightSoft);
    addThemeAction(m_tr.tr(QStringLiteral("theme.lightBright")), ThemeSetting::LightBright);
    addThemeAction(m_tr.tr(QStringLiteral("theme.dark")), ThemeSetting::Dark);
    addThemeAction(m_tr.tr(QStringLiteral("theme.system")), ThemeSetting::System);
    themeMenu->addSeparator();
    themeMenu->addAction(m_tr.tr(QStringLiteral("theme.toggle")), this, &AppShell::themeToggleRequested);
    themeBtn->setMenu(themeMenu);

    auto *helpBtn = new AvarButton(AvarButtonVariant::Ghost, actionsHost);
    helpBtn->setObjectName(QStringLiteral("AvarHeaderHelp"));
    helpBtn->setText(QStringLiteral("?"));
    connect(helpBtn, &QPushButton::clicked, this, [this] {
        setPage(m_page == AppPage::Help ? AppPage::Dashboard : AppPage::Help);
    });

    auto *settingsBtn = new AvarButton(AvarButtonVariant::Ghost, actionsHost);
    settingsBtn->setObjectName(QStringLiteral("AvarHeaderSettings"));
    settingsBtn->setText(QStringLiteral("⚙"));
    connect(settingsBtn, &QPushButton::clicked, this, [this] {
        if (m_page == AppPage::Settings) {
            setPage(AppPage::Dashboard);
        } else {
            setPage(AppPage::Settings);
            emit openSettingsCategory(SettingsCategory::General);
        }
    });

    m_helpHeaderButton = helpBtn;
    m_settingsHeaderButton = settingsBtn;

    actions->addWidget(themeBtn);
    actions->addWidget(helpBtn);
    actions->addWidget(settingsBtn);

    grid->addWidget(brandHost, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(actionsHost, 0, 1, Qt::AlignCenter);
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    auto *controlsHost = new QWidget(m_header);
    auto *controlsLayout = new QHBoxLayout(controlsHost);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->addStretch();
    controlsLayout->addWidget(new WindowControls(controlsHost));
    grid->addWidget(controlsHost, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    m_headerDrag = new HeaderWindowDrag(m_header, this);
#else
    grid->addWidget(new QWidget(m_header), 0, 2);
#endif
}

void AppShell::setPage(AppPage page)
{
    m_page = page;
    m_sidebarBody->setCurrentIndex(static_cast<int>(page));
    if (m_backButton) {
        m_backButton->setVisible(page != AppPage::Dashboard);
    }
    const auto polishActive = [](QPushButton *button, bool active) {
        if (!button) {
            return;
        }
        button->setProperty("active", active);
        button->style()->unpolish(button);
        button->style()->polish(button);
    };
    polishActive(m_helpHeaderButton, page == AppPage::Help);
    polishActive(m_settingsHeaderButton, page == AppPage::Settings);
    emit pageChanged(page);
}

void AppShell::setSettingsCategory(SettingsCategory category)
{
    m_settingsSidebarNav->setCategory(category);
}

void AppShell::setConnectionState(ConnectionState state)
{
    if (m_sessionSelector) {
        m_sessionSelector->setConnectionState(state);
    }
}

QStackedWidget *AppShell::pageStack()
{
    return m_stack;
}

SettingsSidebarNav *AppShell::settingsSidebarNav() const
{
    return m_settingsSidebarNav;
}

QueuePanelWidget *AppShell::queuePanel() const
{
    return m_queuePanel;
}

SessionSelector *AppShell::sessionSelector() const
{
    return m_sessionSelector;
}

} // namespace avar::gui
