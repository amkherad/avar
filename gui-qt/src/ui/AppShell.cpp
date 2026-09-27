#include "ui/AppShell.hpp"

#include "config/LayoutPreferences.hpp"
#include "i18n/Translator.hpp"
#include "session/SessionManager.hpp"
#include "theme/FaIcon.hpp"
#include "theme/ThemeManager.hpp"
#include "ui/widgets/AvarButton.hpp"
#include "ui/widgets/QueuePanelWidget.hpp"
#include "ui/widgets/ResizeHandle.hpp"
#include "ui/widgets/SessionSelector.hpp"
#include "config/AppSettings.hpp"
#include "config/GuiPreferences.hpp"
#include "ui/widgets/ExtensionIntegrationButton.hpp"
#include "ui/widgets/HeaderWindowDrag.hpp"
#include "ui/widgets/HelpSidebarNav.hpp"
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
#include <QApplication>
#include <QPalette>
#include <QVBoxLayout>

namespace avar::gui {

namespace {

constexpr int kHeaderFaIconSize = 14;

QColor headerIconColor(const ThemeManager &theme)
{
    return QColor(theme.currentTokens().textMuted);
}

void applyHeaderIconButton(QPushButton *button, const QIcon &icon)
{
    if (button == nullptr) {
        return;
    }
    button->setText({});
    button->setLayoutDirection(Qt::LeftToRight);
    button->setIcon(icon);
    button->setIconSize(QSize(kHeaderFaIconSize, kHeaderFaIconSize));
}

} // namespace

AppShell::AppShell(Translator &translator,
                   ThemeManager &theme,
                   LayoutPreferences &layout,
                   SessionManager &sessions,
                   ExtensionBridgeClient &extensionBridge,
                   AppSettings &appSettings,
                   GuiPreferences &guiPreferences,
                   QWidget *parent)
    : QWidget(parent)
    , m_tr(translator)
    , m_theme(theme)
    , m_layout(layout)
    , m_sessions(sessions)
    , m_extension(extensionBridge)
    , m_appSettings(appSettings)
    , m_guiPreferences(guiPreferences)
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

    m_queuePanel = new QueuePanelWidget(m_tr, m_theme, m_sidebarBody);

    m_settingsSidebarNav = new SettingsSidebarNav(m_tr, m_sidebarBody);
    m_helpSidebarNav = new HelpSidebarNav(m_tr, m_sidebarBody);

    m_sidebarBody->addWidget(m_queuePanel);
    m_sidebarBody->addWidget(m_settingsSidebarNav);
    m_sidebarBody->addWidget(m_helpSidebarNav);
    sidebarLayout->addWidget(m_sidebarBody, 1);

    m_sessionSelector = new SessionSelector(m_tr, m_sessions, m_sidebar);
    sidebarLayout->addWidget(m_sessionSelector);

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

    connect(&m_theme, &ThemeManager::themeChanged, this, [this] { updateHeaderIcons(); });
    updateHeaderIcons();
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
    connect(m_backButton, &QPushButton::clicked, this, [this] { setPage(AppPage::Dashboard); });

    auto *icon = new QLabel(m_header);
    icon->setObjectName(QStringLiteral("AvarHeaderIcon"));
    icon->setPixmap(QIcon(QStringLiteral(":/icon.svg")).pixmap(24, 24));

    m_headerTitle = new QLabel(m_tr.tr(QStringLiteral("app.title")), m_header);
    m_headerTitle->setObjectName(QStringLiteral("AvarHeaderTitle"));
    m_headerSubtitle = new QLabel(m_tr.tr(QStringLiteral("app.subtitle")), m_header);
    m_headerSubtitle->setObjectName(QStringLiteral("AvarHeaderSubtitle"));

    auto *brandHost = new QWidget(m_header);
    auto *brand = new QHBoxLayout(brandHost);
    brand->setContentsMargins(8, 0, 8, 0);
    brand->addWidget(m_backButton);
    brand->addWidget(icon);
    brand->addWidget(m_headerTitle);
    brand->addWidget(m_headerSubtitle);
    brand->addStretch();

    auto *actionsHost = new QWidget(m_header);
    actionsHost->setObjectName(QStringLiteral("AvarHeaderActions"));
    auto *actions = new QHBoxLayout(actionsHost);
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(4);
#if defined(AVAR_GUI_HOSTING_DESKTOP)
    actions->addWidget(
        new ExtensionIntegrationButton(m_tr, m_theme, m_extension, m_appSettings, m_guiPreferences, actionsHost));
    auto *separator = new QFrame(actionsHost);
    separator->setObjectName(QStringLiteral("AvarHeaderSeparator"));
    separator->setFrameShape(QFrame::VLine);
    actions->addWidget(separator);
#endif

    m_themeButton = new AvarButton(AvarButtonVariant::Ghost, actionsHost);
    m_themeButton->setObjectName(QStringLiteral("AvarHeaderTheme"));
    retranslateThemeMenu();

    auto *helpBtn = new AvarButton(AvarButtonVariant::Ghost, actionsHost);
    helpBtn->setObjectName(QStringLiteral("AvarHeaderHelp"));
    connect(helpBtn, &QPushButton::clicked, this, [this] {
        setPage(m_page == AppPage::Help ? AppPage::Dashboard : AppPage::Help);
    });

    auto *settingsBtn = new AvarButton(AvarButtonVariant::Ghost, actionsHost);
    settingsBtn->setObjectName(QStringLiteral("AvarHeaderSettings"));
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

    actions->addWidget(m_themeButton);
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

HelpSidebarNav *AppShell::helpSidebarNav() const
{
    return m_helpSidebarNav;
}

void AppShell::setHelpTopicId(const QString &id)
{
    if (m_helpSidebarNav != nullptr) {
        m_helpSidebarNav->setTopicId(id);
    }
}

QueuePanelWidget *AppShell::queuePanel() const
{
    return m_queuePanel;
}

SessionSelector *AppShell::sessionSelector() const
{
    return m_sessionSelector;
}

void AppShell::retranslateThemeMenu()
{
    if (m_themeButton == nullptr) {
        return;
    }
    auto *themeMenu = new QMenu(m_themeButton);
    const auto addThemeAction = [this, themeMenu](const QString &label, ThemeSetting setting) {
        auto *action = themeMenu->addAction(label);
        connect(action, &QAction::triggered, this, [this, setting] { emit themeSettingRequested(setting); });
    };
    addThemeAction(m_tr.tr(QStringLiteral("theme.lightSoft")), ThemeSetting::LightSoft);
    addThemeAction(m_tr.tr(QStringLiteral("theme.lightBright")), ThemeSetting::LightBright);
    addThemeAction(m_tr.tr(QStringLiteral("theme.queenMode")), ThemeSetting::QueenMode);
    addThemeAction(m_tr.tr(QStringLiteral("theme.dark")), ThemeSetting::Dark);
    addThemeAction(m_tr.tr(QStringLiteral("theme.system")), ThemeSetting::System);
    themeMenu->addSeparator();
    themeMenu->addAction(m_tr.tr(QStringLiteral("theme.toggle")), this, &AppShell::themeToggleRequested);
    m_themeButton->setMenu(themeMenu);
}

void AppShell::updateHeaderIcons()
{
    const QColor color = headerIconColor(m_theme);
    applyHeaderIconButton(m_backButton, FaIcon::solidBack(kHeaderFaIconSize, color, m_tr.isRtl()));
    const bool darkResolved = m_theme.currentTokens().id == QStringLiteral("dark");
    applyHeaderIconButton(m_themeButton,
                          FaIcon::solid(darkResolved ? QStringLiteral("lightbulb") : QStringLiteral("moon"),
                                        kHeaderFaIconSize, color));
    applyHeaderIconButton(m_helpHeaderButton,
                          FaIcon::solid(QStringLiteral("circle-question"), kHeaderFaIconSize, color));
    applyHeaderIconButton(m_settingsHeaderButton, FaIcon::solid(QStringLiteral("gear"), kHeaderFaIconSize, color));
}

void AppShell::retranslateUi()
{
    if (m_headerTitle != nullptr) {
        m_headerTitle->setText(m_tr.tr(QStringLiteral("app.title")));
    }
    if (m_headerSubtitle != nullptr) {
        m_headerSubtitle->setText(m_tr.tr(QStringLiteral("app.subtitle")));
    }
    retranslateThemeMenu();
    updateHeaderIcons();
    if (m_settingsSidebarNav != nullptr) {
        m_settingsSidebarNav->retranslateUi();
    }
    if (m_helpSidebarNav != nullptr) {
        m_helpSidebarNav->retranslateUi();
    }
    if (m_queuePanel != nullptr) {
        m_queuePanel->retranslateUi();
    }
}

} // namespace avar::gui
