#include "ui/settings/SettingsPanels.hpp"

#include "i18n/LocaleCatalog.hpp"

#include "ui/settings/SettingsLayout.hpp"
#include "ui/settings/SettingsPanelsImpl.hpp"

#include "api/DaemonClient.hpp"
#include "config/AppSettings.hpp"
#include "config/GuiPreferences.hpp"
#include "extension/ExtensionBridgeClient.hpp"
#include "i18n/Translator.hpp"
#include "session/SessionManager.hpp"
#include "theme/ThemeTokens.hpp"
#include "ui/settings/SettingsContext.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QSignalBlocker>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <memory>

namespace avar::gui {

namespace {

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("class", QStringLiteral("AvarSectionTitle"));
    return label;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setProperty("class", QStringLiteral("AvarSettingsHint"));
    return label;
}

QWidget *pathField(Translator &tr,
                   const QString &label,
                   const QString &initial,
                   const std::function<void(const QString &)> &onChange,
                   QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *edit = new QLineEdit(initial, row);
    auto *browse = new AvarButton(AvarButtonVariant::Secondary, row);
    browse->setText(tr.tr(QStringLiteral("common.browse")));
    QObject::connect(browse, &QPushButton::clicked, row, [edit, &tr] {
        const QString dir = QFileDialog::getExistingDirectory(nullptr, tr.tr(QStringLiteral("common.browse")));
        if (!dir.isEmpty()) {
            edit->setText(dir);
        }
    });
    QObject::connect(edit, &QLineEdit::editingFinished, row, [edit, onChange] { onChange(edit->text()); });
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    auto *wrap = new QWidget(parent);
    auto *form = new QFormLayout(wrap);
    form->addRow(label, row);
    return wrap;
}

QWidget *buildGeneralPanel(const SettingsContext &ctx, QWidget *parent)
{
    auto *panel = new QWidget(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.categories.general")), panel));

    auto *theme = new QComboBox(panel);
    theme->addItem(ctx.translator.tr(QStringLiteral("settings.themeLight")), static_cast<int>(ThemeSetting::LightSoft));
    theme->addItem(ctx.translator.tr(QStringLiteral("settings.themeLightBright")),
                   static_cast<int>(ThemeSetting::LightBright));
    theme->addItem(ctx.translator.tr(QStringLiteral("settings.themeQueenMode")),
                   static_cast<int>(ThemeSetting::QueenMode));
    theme->addItem(ctx.translator.tr(QStringLiteral("settings.themeDark")), static_cast<int>(ThemeSetting::Dark));
    theme->addItem(ctx.translator.tr(QStringLiteral("settings.themeSystem")), static_cast<int>(ThemeSetting::System));
    for (int i = 0; i < theme->count(); ++i) {
        if (theme->itemData(i).toInt() == static_cast<int>(ctx.appSettings.themeSetting())) {
            theme->setCurrentIndex(i);
            break;
        }
    }
    QObject::connect(theme, &QComboBox::currentIndexChanged, panel, [&ctx, theme] {
        ctx.appSettings.setThemeSetting(static_cast<ThemeSetting>(theme->currentData().toInt()));
    });
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.theme")), panel));
    layout->addWidget(theme);

    auto *locale = new QComboBox(panel);
    for (const LocaleDescriptor &entry : LocaleCatalog::availableLocales()) {
        locale->addItem(entry.nameNative, entry.id);
    }
    const int localeIndex = locale->findData(ctx.appSettings.locale());
    {
        QSignalBlocker blocker(locale);
        locale->setCurrentIndex(localeIndex >= 0 ? localeIndex : 0);
    }
    QObject::connect(locale, &QComboBox::currentIndexChanged, panel, [&ctx, locale](int index) {
        if (index < 0) {
            return;
        }
        const QString localeId = locale->itemData(index).toString();
        ctx.appSettings.setLocale(localeId);
        ctx.translator.setLocale(localeId);
    });
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.language")), panel));
    layout->addWidget(locale);

    auto *sync = new QComboBox(panel);
    sync->addItem(ctx.translator.tr(QStringLiteral("settings.syncPoll")), static_cast<int>(SyncChannel::Poll));
    sync->addItem(ctx.translator.tr(QStringLiteral("settings.syncSse")), static_cast<int>(SyncChannel::Sse));
    sync->addItem(ctx.translator.tr(QStringLiteral("settings.syncWebSocket")), static_cast<int>(SyncChannel::WebSocket));
    for (int i = 0; i < sync->count(); ++i) {
        if (sync->itemData(i).toInt() == static_cast<int>(ctx.guiPreferences.syncChannel())) {
            sync->setCurrentIndex(i);
            break;
        }
    }
    auto *refreshSpin = new QSpinBox(panel);
    refreshSpin->setRange(1, 120);
    refreshSpin->setValue(ctx.guiPreferences.refreshIntervalMs() / 1000);
    refreshSpin->setVisible(ctx.guiPreferences.syncChannel() == SyncChannel::Poll);
    QObject::connect(sync, &QComboBox::currentIndexChanged, panel, [&ctx, sync, refreshSpin] {
        const auto channel = static_cast<SyncChannel>(sync->currentData().toInt());
        ctx.guiPreferences.setSyncChannel(channel);
        refreshSpin->setVisible(channel == SyncChannel::Poll);
    });
    QObject::connect(refreshSpin, &QSpinBox::valueChanged, panel, [&ctx](int seconds) {
        ctx.guiPreferences.setRefreshIntervalMs(seconds * 1000);
    });
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.syncChannel")), panel));
    layout->addWidget(sync);
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.refresh")), panel));
    layout->addWidget(refreshSpin);

    auto *pingSpin = new QSpinBox(panel);
    pingSpin->setRange(1, 30);
    pingSpin->setValue(ctx.guiPreferences.pingIntervalMs() / 1000);
    QObject::connect(pingSpin, &QSpinBox::valueChanged, panel, [&ctx](int seconds) {
        ctx.guiPreferences.setPingIntervalMs(seconds * 1000);
    });
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.pingInterval")), panel));
    layout->addWidget(pingSpin);

    auto *dblClick = new QComboBox(panel);
    dblClick->addItem(ctx.translator.tr(QStringLiteral("settings.downloadDoubleClickOpenDetails")),
                      static_cast<int>(DownloadDoubleClickAction::OpenDetails));
    dblClick->addItem(ctx.translator.tr(QStringLiteral("settings.downloadDoubleClickOpenFile")),
                      static_cast<int>(DownloadDoubleClickAction::OpenFile));
    dblClick->setCurrentIndex(ctx.guiPreferences.downloadDoubleClickAction() == DownloadDoubleClickAction::OpenFile ? 1
                                                                                                                     : 0);
    QObject::connect(dblClick, &QComboBox::currentIndexChanged, panel, [&ctx, dblClick] {
        ctx.guiPreferences.setDownloadDoubleClickAction(
            static_cast<DownloadDoubleClickAction>(dblClick->currentData().toInt()));
    });
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.downloadDoubleClick")), panel));
    layout->addWidget(dblClick);

    auto *byteUnit = new QComboBox(panel);
    byteUnit->addItem(ctx.translator.tr(QStringLiteral("settings.byteDisplayUnitBinary")),
                      static_cast<int>(ByteDisplayUnit::Binary));
    byteUnit->addItem(ctx.translator.tr(QStringLiteral("settings.byteDisplayUnitDecimal")),
                      static_cast<int>(ByteDisplayUnit::Decimal));
    byteUnit->setCurrentIndex(ctx.guiPreferences.byteDisplayUnit() == ByteDisplayUnit::Decimal ? 1 : 0);
    QObject::connect(byteUnit, &QComboBox::currentIndexChanged, panel, [&ctx, byteUnit] {
        ctx.guiPreferences.setByteDisplayUnit(static_cast<ByteDisplayUnit>(byteUnit->currentData().toInt()));
    });
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.byteDisplayUnit")), panel));
    layout->addWidget(byteUnit);

    auto *rateUnit = new QComboBox(panel);
    rateUnit->addItem(ctx.translator.tr(QStringLiteral("settings.transferRateDisplayUnitBinaryBytes")),
                      static_cast<int>(TransferRateDisplayUnit::BinaryBytes));
    rateUnit->addItem(ctx.translator.tr(QStringLiteral("settings.transferRateDisplayUnitBinaryBits")),
                      static_cast<int>(TransferRateDisplayUnit::BinaryBits));
    rateUnit->setCurrentIndex(ctx.guiPreferences.transferRateDisplayUnit() == TransferRateDisplayUnit::BinaryBits ? 1
                                                                                                                    : 0);
    QObject::connect(rateUnit, &QComboBox::currentIndexChanged, panel, [&ctx, rateUnit] {
        ctx.guiPreferences.setTransferRateDisplayUnit(
            static_cast<TransferRateDisplayUnit>(rateUnit->currentData().toInt()));
    });
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.transferRateDisplayUnit")), panel));
    layout->addWidget(rateUnit);

    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.remoteCopy.title")), panel));
    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("settings.remoteCopy.hint")), panel));
    layout->addWidget(pathField(ctx.translator,
                                ctx.translator.tr(QStringLiteral("settings.remoteCopy.localDownloadPath")),
                                ctx.guiPreferences.localDownloadPath(),
                                [&ctx](const QString &path) { ctx.guiPreferences.setLocalDownloadPath(path); },
                                panel));

    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.footerMonitors")), panel));
    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("settings.footerMonitorsHint")), panel));
    auto monitors = ctx.guiPreferences.footerMonitors();
    auto *footerDisplay = new QComboBox(panel);
    footerDisplay->addItem(ctx.translator.tr(QStringLiteral("settings.footerMonitorDisplayText")),
                             static_cast<int>(FooterMonitorDisplay::Text));
    footerDisplay->addItem(ctx.translator.tr(QStringLiteral("settings.footerMonitorDisplayHistogram")),
                           static_cast<int>(FooterMonitorDisplay::Histogram));
    footerDisplay->setCurrentIndex(monitors.display == FooterMonitorDisplay::Histogram ? 1 : 0);
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.footerMonitorDisplay")), panel));
    layout->addWidget(footerDisplay);
    auto addMonitorCheck = [&](const QString &key, bool checked) {
        auto *box = new QCheckBox(ctx.translator.tr(key), panel);
        box->setChecked(checked);
        QObject::connect(box, &QCheckBox::toggled, panel, [&ctx, key, box, footerDisplay] {
            FooterMonitorSettings m = ctx.guiPreferences.footerMonitors();
            if (key == QStringLiteral("settings.footerMonitor.disk")) {
                m.disk = box->isChecked();
            } else if (key == QStringLiteral("settings.footerMonitor.memory")) {
                m.memory = box->isChecked();
            } else if (key == QStringLiteral("settings.footerMonitor.cpu")) {
                m.cpu = box->isChecked();
            } else if (key == QStringLiteral("settings.footerMonitor.network")) {
                m.network = box->isChecked();
            }
            m.display = static_cast<FooterMonitorDisplay>(footerDisplay->currentData().toInt());
            ctx.guiPreferences.setFooterMonitors(m);
        });
        QObject::connect(footerDisplay, &QComboBox::currentIndexChanged, panel, [&ctx, footerDisplay] {
            FooterMonitorSettings m = ctx.guiPreferences.footerMonitors();
            m.display = static_cast<FooterMonitorDisplay>(footerDisplay->currentData().toInt());
            ctx.guiPreferences.setFooterMonitors(m);
        });
        layout->addWidget(box);
    };
    addMonitorCheck(QStringLiteral("settings.footerMonitor.disk"), monitors.disk);
    addMonitorCheck(QStringLiteral("settings.footerMonitor.memory"), monitors.memory);
    addMonitorCheck(QStringLiteral("settings.footerMonitor.cpu"), monitors.cpu);
    addMonitorCheck(QStringLiteral("settings.footerMonitor.network"), monitors.network);

    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.notifications.title")), panel));
    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("settings.notifications.hint")), panel));
    auto *notifications = new QCheckBox(ctx.translator.tr(QStringLiteral("settings.notifications.enabled")), panel);
    notifications->setChecked(ctx.guiPreferences.notificationsEnabled());
    QObject::connect(notifications, &QCheckBox::toggled, &ctx.guiPreferences, &GuiPreferences::setNotificationsEnabled);
    layout->addWidget(notifications);

#if defined(AVAR_GUI_HOSTING_DESKTOP)
    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.desktop.title")), panel));
    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("settings.desktop.hint")), panel));
    auto *keepTray = new QCheckBox(ctx.translator.tr(QStringLiteral("settings.desktop.keepInTrayOnClose")), panel);
    keepTray->setChecked(ctx.appSettings.keepInTrayOnClose());
    QObject::connect(keepTray, &QCheckBox::toggled, &ctx.appSettings, &AppSettings::setKeepInTrayOnClose);
    layout->addWidget(keepTray);
#endif

    layout->addStretch();
    return wrapSettingsPage(panel);
}

} // namespace

void populateSettingsStack(QStackedWidget *stack, const SettingsContext &ctx, QWidget *parent)
{
    stack->addWidget(buildGeneralPanel(ctx, parent));
    stack->addWidget(buildDownloadSettingsPanel(ctx, parent));
    stack->addWidget(buildQueuesSettingsPanel(ctx, parent));
    stack->addWidget(buildDaemonSettingsPanel(ctx, parent));
    stack->addWidget(buildBrowserSettingsPanel(ctx, parent));
    stack->addWidget(buildShortcutsSettingsPanel(ctx, parent));
    stack->addWidget(buildAboutSettingsPanel(ctx, parent));
}

void showSettingsCategory(QStackedWidget *stack, SettingsCategory category)
{
    stack->setCurrentIndex(static_cast<int>(category));
}

} // namespace avar::gui
