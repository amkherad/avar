#include "ui/settings/SettingsPanelsImpl.hpp"

#include "api/DaemonClient.hpp"
#include "api/DaemonTypes.hpp"
#include "config/AppSettings.hpp"
#include "config/GuiPreferences.hpp"
#include "config/ShortcutDefinitions.hpp"
#include "extension/ExtensionBridgeClient.hpp"
#include "i18n/Translator.hpp"
#include "session/SessionManager.hpp"
#include "ui/settings/SettingsContext.hpp"
#include "ui/widgets/AvarButton.hpp"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <functional>
#include <memory>

namespace avar::gui {

namespace {

QWidget *wrapScroll(QWidget *content)
{
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

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

QLabel *statusLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setWordWrap(true);
    label->setProperty("class", QStringLiteral("AvarSettingsHint"));
    return label;
}

void loadConfigKeys(DaemonClient &daemon,
                    const QStringList &keys,
                    const QHash<QString, QString> &defaults,
                    const std::function<void(const QHash<QString, QString> &)> &done)
{
    auto *values = new QHash<QString, QString>();
    auto *index = new int(0);
    auto step = std::make_shared<std::function<void()>>();
    *step = [&daemon, keys, defaults, values, index, done, step] {
        if (*index >= keys.size()) {
            done(*values);
            delete values;
            delete index;
            return;
        }
        const QString key = keys.at(*index);
        daemon.getConfig(key, defaults.value(key), [values, index, key, step](const QString &value) {
            values->insert(key, value);
            ++(*index);
            (*step)();
        });
    };
    (*step)();
}

void saveConfigKeys(DaemonClient &daemon,
                    const QHash<QString, QString> &values,
                    const std::function<void(bool, const QString &)> &done)
{
    const QStringList keys = values.keys();
    auto *index = new int(0);
    auto step = std::make_shared<std::function<void()>>();
    *step = [&daemon, values, keys, index, done, step] {
        if (*index >= keys.size()) {
            done(true, {});
            delete index;
            return;
        }
        const QString key = keys.at(*index);
        daemon.setConfig(key, values.value(key), [index, done, step](bool ok, const QString &err) {
            if (!ok) {
                done(false, err);
                delete index;
                return;
            }
            ++(*index);
            (*step)();
        });
    };
    (*step)();
}

QWidget *pathRow(Translator &tr, const QString &label, QLineEdit *&editOut, QWidget *parent)
{
    auto *wrap = new QWidget(parent);
    auto *layout = new QHBoxLayout(wrap);
    layout->setContentsMargins(0, 0, 0, 0);
    editOut = new QLineEdit(wrap);
    auto *browse = new AvarButton(AvarButtonVariant::Secondary, wrap);
    browse->setText(tr.tr(QStringLiteral("common.browse")));
    QObject::connect(browse, &QPushButton::clicked, wrap, [editOut, &tr] {
        const QString dir = QFileDialog::getExistingDirectory(nullptr, tr.tr(QStringLiteral("common.browse")));
        if (!dir.isEmpty()) {
            editOut->setText(dir);
        }
    });
    layout->addWidget(editOut, 1);
    layout->addWidget(browse);
    auto *formHost = new QWidget(parent);
    auto *form = new QFormLayout(formHost);
    form->addRow(label, wrap);
    return formHost;
}

class ShortcutCaptureFilter final : public QObject {
public:
    explicit ShortcutCaptureFilter(const std::function<void(const QString &)> &onCaptured, QObject *parent = nullptr)
        : QObject(parent)
        , m_onCaptured(onCaptured)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::KeyPress) {
            return QObject::eventFilter(watched, event);
        }
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            watched->removeEventFilter(this);
            deleteLater();
            return true;
        }
        QStringList parts;
        if (keyEvent->modifiers() & Qt::ControlModifier || keyEvent->modifiers() & Qt::MetaModifier) {
            parts.append(QStringLiteral("ctrl"));
        }
        if (keyEvent->modifiers() & Qt::ShiftModifier) {
            parts.append(QStringLiteral("shift"));
        }
        if (keyEvent->modifiers() & Qt::AltModifier) {
            parts.append(QStringLiteral("alt"));
        }
        QString key = QKeySequence(keyEvent->key()).toString(QKeySequence::NativeText).toLower();
        if (keyEvent->key() == Qt::Key_Space) {
            key = QStringLiteral("space");
        } else if (keyEvent->key() == Qt::Key_Delete) {
            key = QStringLiteral("delete");
        } else if (key.size() == 1) {
            key = keyEvent->text().toLower();
        }
        if (key == QStringLiteral("control") || key == QStringLiteral("shift") || key == QStringLiteral("alt")
            || key == QStringLiteral("meta")) {
            return true;
        }
        parts.append(key);
        m_onCaptured(parts.join(QLatin1Char('+')));
        watched->removeEventFilter(this);
        deleteLater();
        return true;
    }

private:
    std::function<void(const QString &)> m_onCaptured;
};

} // namespace

QWidget *buildDownloadSettingsPanel(const SettingsContext &ctx, QWidget *parent)
{
    auto *panel = new QWidget(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);

    static const QHash<QString, QString> defaults = {
        {QStringLiteral("dm.segmentation.enabled"), QStringLiteral("true")},
        {QStringLiteral("dm.segmentation.strategy"), QStringLiteral("balanced")},
        {QStringLiteral("dm.segmentation.concurrency"), QStringLiteral("4")},
        {QStringLiteral("dm.segmentation.chunkSize"), QStringLiteral("262144")},
        {QStringLiteral("dm.segmentation.minFileSize"), QStringLiteral("1048576")},
        {QStringLiteral("dm.tempPath"), QStringLiteral("")},
        {QStringLiteral("dm.downloadPath"), QStringLiteral("")},
        {QStringLiteral("dm.progress.sizeUnit"), QStringLiteral("MiB")},
        {QStringLiteral("dm.progress.speedUnit"), QStringLiteral("MiB/s")},
        {QStringLiteral("dm.progress.style"), QStringLiteral("segmented")},
        {QStringLiteral("dm.proxy.enabled"), QStringLiteral("false")},
        {QStringLiteral("dm.proxy.type"), QStringLiteral("http")},
        {QStringLiteral("dm.proxy.host"), QStringLiteral("")},
        {QStringLiteral("dm.proxy.port"), QStringLiteral("")},
        {QStringLiteral("dm.proxy.username"), QStringLiteral("")},
        {QStringLiteral("dm.proxy.password"), QStringLiteral("")},
        {QStringLiteral("dm.proxy.noProxy"), QStringLiteral("")},
    };

    QLineEdit *tempPath = nullptr;
    QLineEdit *downloadPath = nullptr;
    layout->addWidget(pathRow(ctx.translator, ctx.translator.tr(QStringLiteral("settings.download.tempPath")), tempPath,
                              panel));
    layout->addWidget(
        pathRow(ctx.translator, ctx.translator.tr(QStringLiteral("settings.download.downloadPath")), downloadPath, panel));

    auto *segEnabled = new QCheckBox(ctx.translator.tr(QStringLiteral("settings.download.segmentationEnabled")), panel);
    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.download.segmentation")), panel));
    layout->addWidget(segEnabled);

    auto *strategy = new QComboBox(panel);
    strategy->addItem(ctx.translator.tr(QStringLiteral("settings.download.strategyBalanced")), QStringLiteral("balanced"));
    strategy->addItem(ctx.translator.tr(QStringLiteral("settings.download.strategyLeftHeavy")),
                      QStringLiteral("left-heavy"));
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.download.segmentationStrategy")), panel));
    layout->addWidget(strategy);

    auto *concurrency = new QLineEdit(panel);
    auto *chunkSize = new QLineEdit(panel);
    auto *minFileSize = new QLineEdit(panel);
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.download.concurrency")), panel));
    layout->addWidget(concurrency);
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.download.chunkSize")), panel));
    layout->addWidget(chunkSize);
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.download.minFileSize")), panel));
    layout->addWidget(minFileSize);

    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.download.progress")), panel));
    auto *sizeUnit = new QComboBox(panel);
    for (const QString &unit : {QStringLiteral("Bytes"), QStringLiteral("KiB"), QStringLiteral("MiB"),
                                QStringLiteral("GiB")}) {
        sizeUnit->addItem(unit, unit);
    }
    auto *speedUnit = new QComboBox(panel);
    for (const QString &unit :
         {QStringLiteral("Bytes/s"), QStringLiteral("bits/s"), QStringLiteral("KiB/s"), QStringLiteral("MiB/s"),
          QStringLiteral("GiB/s"), QStringLiteral("Kib/s"), QStringLiteral("Mib/s"), QStringLiteral("Gib/s")}) {
        speedUnit->addItem(unit, unit);
    }
    auto *progressStyle = new QComboBox(panel);
    progressStyle->addItem(ctx.translator.tr(QStringLiteral("settings.download.progressSegmented")),
                           QStringLiteral("segmented"));
    progressStyle->addItem(ctx.translator.tr(QStringLiteral("settings.download.progressAggregate")),
                           QStringLiteral("aggregate"));
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.download.sizeUnit")), panel));
    layout->addWidget(sizeUnit);
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.download.speedUnit")), panel));
    layout->addWidget(speedUnit);
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.download.progressStyle")), panel));
    layout->addWidget(progressStyle);

    auto *proxyGroup = new QGroupBox(ctx.translator.tr(QStringLiteral("proxy.title")), panel);
    auto *proxyLayout = new QVBoxLayout(proxyGroup);
    auto *proxyEnabled = new QCheckBox(ctx.translator.tr(QStringLiteral("proxy.enabled")), proxyGroup);
    proxyLayout->addWidget(proxyEnabled);
    auto *proxyType = new QComboBox(proxyGroup);
    proxyType->addItem(ctx.translator.tr(QStringLiteral("proxy.types.http")), QStringLiteral("http"));
    proxyType->addItem(ctx.translator.tr(QStringLiteral("proxy.types.https")), QStringLiteral("https"));
    proxyType->addItem(ctx.translator.tr(QStringLiteral("proxy.types.socks5")), QStringLiteral("socks5"));
    auto *proxyHost = new QLineEdit(proxyGroup);
    auto *proxyPort = new QLineEdit(proxyGroup);
    auto *proxyUser = new QLineEdit(proxyGroup);
    auto *proxyPass = new QLineEdit(proxyGroup);
    proxyPass->setEchoMode(QLineEdit::Password);
    auto *proxyNoProxy = new QLineEdit(proxyGroup);
    proxyLayout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("proxy.type")), proxyGroup));
    proxyLayout->addWidget(proxyType);
    proxyLayout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("proxy.host")), proxyGroup));
    proxyLayout->addWidget(proxyHost);
    proxyLayout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("proxy.port")), proxyGroup));
    proxyLayout->addWidget(proxyPort);
    proxyLayout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("proxy.username")), proxyGroup));
    proxyLayout->addWidget(proxyUser);
    proxyLayout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("proxy.password")), proxyGroup));
    proxyLayout->addWidget(proxyPass);
    proxyLayout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("proxy.noProxy")), proxyGroup));
    proxyLayout->addWidget(proxyNoProxy);
    layout->addWidget(proxyGroup);

    auto *status = statusLabel(panel);
    layout->addWidget(status);
    auto *saveBtn = new AvarButton(AvarButtonVariant::Primary, panel);
    saveBtn->setText(ctx.translator.tr(QStringLiteral("common.save")));
    layout->addWidget(saveBtn);
    layout->addStretch();

    const QStringList allKeys = defaults.keys();
    loadConfigKeys(ctx.daemon, allKeys, defaults, [=](const QHash<QString, QString> &values) {
        tempPath->setText(values.value(QStringLiteral("dm.tempPath")));
        downloadPath->setText(values.value(QStringLiteral("dm.downloadPath")));
        segEnabled->setChecked(values.value(QStringLiteral("dm.segmentation.enabled")) == QStringLiteral("true"));
        const int stratIdx = strategy->findData(values.value(QStringLiteral("dm.segmentation.strategy")));
        if (stratIdx >= 0) {
            strategy->setCurrentIndex(stratIdx);
        }
        concurrency->setText(values.value(QStringLiteral("dm.segmentation.concurrency")));
        chunkSize->setText(values.value(QStringLiteral("dm.segmentation.chunkSize")));
        minFileSize->setText(values.value(QStringLiteral("dm.segmentation.minFileSize")));
        const int su = sizeUnit->findData(values.value(QStringLiteral("dm.progress.sizeUnit")));
        if (su >= 0) {
            sizeUnit->setCurrentIndex(su);
        }
        const int sp = speedUnit->findData(values.value(QStringLiteral("dm.progress.speedUnit")));
        if (sp >= 0) {
            speedUnit->setCurrentIndex(sp);
        }
        const int ps = progressStyle->findData(values.value(QStringLiteral("dm.progress.style")));
        if (ps >= 0) {
            progressStyle->setCurrentIndex(ps);
        }
        proxyEnabled->setChecked(values.value(QStringLiteral("dm.proxy.enabled")) == QStringLiteral("true"));
        const int pt = proxyType->findData(values.value(QStringLiteral("dm.proxy.type")));
        if (pt >= 0) {
            proxyType->setCurrentIndex(pt);
        }
        proxyHost->setText(values.value(QStringLiteral("dm.proxy.host")));
        proxyPort->setText(values.value(QStringLiteral("dm.proxy.port")));
        proxyUser->setText(values.value(QStringLiteral("dm.proxy.username")));
        proxyPass->setText(values.value(QStringLiteral("dm.proxy.password")));
        proxyNoProxy->setText(values.value(QStringLiteral("dm.proxy.noProxy")));
    });

    QObject::connect(saveBtn, &QPushButton::clicked, panel, [=, &ctx] {
        QHash<QString, QString> values;
        values.insert(QStringLiteral("dm.tempPath"), tempPath->text());
        values.insert(QStringLiteral("dm.downloadPath"), downloadPath->text());
        values.insert(QStringLiteral("dm.segmentation.enabled"), segEnabled->isChecked() ? QStringLiteral("true")
                                                                                       : QStringLiteral("false"));
        values.insert(QStringLiteral("dm.segmentation.strategy"), strategy->currentData().toString());
        values.insert(QStringLiteral("dm.segmentation.concurrency"), concurrency->text());
        values.insert(QStringLiteral("dm.segmentation.chunkSize"), chunkSize->text());
        values.insert(QStringLiteral("dm.segmentation.minFileSize"), minFileSize->text());
        values.insert(QStringLiteral("dm.progress.sizeUnit"), sizeUnit->currentData().toString());
        values.insert(QStringLiteral("dm.progress.speedUnit"), speedUnit->currentData().toString());
        values.insert(QStringLiteral("dm.progress.style"), progressStyle->currentData().toString());
        values.insert(QStringLiteral("dm.proxy.enabled"), proxyEnabled->isChecked() ? QStringLiteral("true")
                                                                                    : QStringLiteral("false"));
        values.insert(QStringLiteral("dm.proxy.type"), proxyType->currentData().toString());
        values.insert(QStringLiteral("dm.proxy.host"), proxyHost->text());
        values.insert(QStringLiteral("dm.proxy.port"), proxyPort->text());
        values.insert(QStringLiteral("dm.proxy.username"), proxyUser->text());
        values.insert(QStringLiteral("dm.proxy.password"), proxyPass->text());
        values.insert(QStringLiteral("dm.proxy.noProxy"), proxyNoProxy->text());
        saveBtn->setEnabled(false);
        status->clear();
        saveConfigKeys(ctx.daemon, values, [=](bool ok, const QString &err) {
            saveBtn->setEnabled(true);
            if (ok) {
                status->setText(ctx.translator.tr(QStringLiteral("settings.saved")));
            } else {
                status->setText(err.isEmpty() ? ctx.translator.tr(QStringLiteral("common.error")) : err);
            }
        });
    });

    return wrapScroll(panel);
}

QWidget *buildQueuesSettingsPanel(const SettingsContext &ctx, QWidget *parent)
{
    auto *panel = new QWidget(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("settings.queues.hint")), panel));

    auto *list = new QListWidget(panel);
    layout->addWidget(list, 1);

    auto *row = new QHBoxLayout();
    auto *refreshBtn = new AvarButton(AvarButtonVariant::Secondary, panel);
    refreshBtn->setText(ctx.translator.tr(QStringLiteral("common.refresh")));
    auto *startBtn = new AvarButton(AvarButtonVariant::Secondary, panel);
    startBtn->setText(ctx.translator.tr(QStringLiteral("queue.start")));
    auto *stopBtn = new AvarButton(AvarButtonVariant::Secondary, panel);
    stopBtn->setText(ctx.translator.tr(QStringLiteral("queue.stop")));
    auto *addBtn = new AvarButton(AvarButtonVariant::Primary, panel);
    addBtn->setText(ctx.translator.tr(QStringLiteral("queue.add")));
    row->addWidget(refreshBtn);
    row->addWidget(startBtn);
    row->addWidget(stopBtn);
    row->addStretch();
    row->addWidget(addBtn);
    layout->addLayout(row);

    auto refresh = [list, &ctx] {
        ctx.daemon.listQueues([list](bool ok, QVector<QueueInfo> queues, QString) {
            list->clear();
            if (!ok) {
                return;
            }
            for (const QueueInfo &q : queues) {
                const QString line =
                    QStringLiteral("%1 — %2%3")
                        .arg(q.name, q.id, q.running ? QStringLiteral(" (running)") : QString());
                auto *item = new QListWidgetItem(line);
                item->setData(Qt::UserRole, q.id);
                list->addItem(item);
            }
        });
    };
    refresh();
    QObject::connect(refreshBtn, &QPushButton::clicked, panel, refresh);
    QObject::connect(startBtn, &QPushButton::clicked, panel, [list, &ctx, refresh, panel] {
        if (auto *item = list->currentItem()) {
            ctx.daemon.startQueue(item->data(Qt::UserRole).toString());
            QTimer::singleShot(400, panel, refresh);
        }
    });
    QObject::connect(stopBtn, &QPushButton::clicked, panel, [list, &ctx, refresh, panel] {
        if (auto *item = list->currentItem()) {
            ctx.daemon.stopQueue(item->data(Qt::UserRole).toString());
            QTimer::singleShot(400, panel, refresh);
        }
    });
    QObject::connect(addBtn, &QPushButton::clicked, panel, [&ctx, refresh, panel] {
        const QString name = QInputDialog::getText(panel, ctx.translator.tr(QStringLiteral("queue.add")),
                                                   ctx.translator.tr(QStringLiteral("queue.nameLabel")));
        if (name.isEmpty()) {
            return;
        }
        QJsonObject params;
        params.insert(QStringLiteral("name"), name);
        ctx.daemon.addQueue(params, [refresh](bool, const QString &, const QString &) { refresh(); });
    });

    return wrapScroll(panel);
}

QWidget *buildDaemonSettingsPanel(const SettingsContext &ctx, QWidget *parent)
{
    auto *panel = new QWidget(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);

    auto *autoShutdown = new QComboBox(panel);
    autoShutdown->addItem(ctx.translator.tr(QStringLiteral("settings.daemon.autoShutdownNever")),
                            QStringLiteral("never"));
    autoShutdown->addItem(ctx.translator.tr(QStringLiteral("settings.daemon.autoShutdownWhenIdle")),
                          QStringLiteral("whenIdle"));
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.daemon.autoShutdown")), panel));
    layout->addWidget(autoShutdown);

    auto *idleSeconds = new QLineEdit(panel);
    layout->addWidget(new QLabel(ctx.translator.tr(QStringLiteral("settings.daemon.autoShutdownIdleSeconds")), panel));
    layout->addWidget(idleSeconds);

    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.daemon.fileLogging")), panel));
    auto *logEnabled = new QCheckBox(ctx.translator.tr(QStringLiteral("settings.daemon.logEnabled")), panel);
    layout->addWidget(logEnabled);
    QLineEdit *logPath = nullptr;
    layout->addWidget(
        pathRow(ctx.translator, ctx.translator.tr(QStringLiteral("settings.daemon.logPath")), logPath, panel));

    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.daemon.fsBrowse")), panel));
    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("settings.daemon.fsBrowseHint")), panel));
    auto *fsBrowse = new QCheckBox(ctx.translator.tr(QStringLiteral("settings.daemon.fsBrowseEnabled")), panel);
    layout->addWidget(fsBrowse);

    layout->addWidget(sectionTitle(ctx.translator.tr(QStringLiteral("settings.daemon.remoteFileDownload")), panel));
    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("settings.daemon.remoteFileDownloadHint")), panel));
    auto *fileDownload = new QCheckBox(ctx.translator.tr(QStringLiteral("settings.daemon.fileDownloadEnabled")), panel);
    layout->addWidget(fileDownload);

    auto *status = statusLabel(panel);
    layout->addWidget(status);
    auto *saveBtn = new AvarButton(AvarButtonVariant::Primary, panel);
    saveBtn->setText(ctx.translator.tr(QStringLiteral("common.save")));
    layout->addWidget(saveBtn);
    layout->addStretch();

    auto updateIdleVisible = [autoShutdown, idleSeconds] {
        idleSeconds->setVisible(autoShutdown->currentData().toString() == QStringLiteral("whenIdle"));
    };
    QObject::connect(autoShutdown, &QComboBox::currentIndexChanged, panel, updateIdleVisible);

    static const QHash<QString, QString> defaults = {
        {QStringLiteral("daemon.server.autoShutdown"), QStringLiteral("never")},
        {QStringLiteral("daemon.server.autoShutdownIdleSeconds"), QStringLiteral("60")},
        {QStringLiteral("log.file.enabled"), QStringLiteral("false")},
        {QStringLiteral("log.file.path"), QStringLiteral("")},
        {QStringLiteral("daemon.server.fileDownload.enabled"), QStringLiteral("false")},
        {QStringLiteral("daemon.server.fsBrowse.enabled"), QStringLiteral("false")},
    };
    loadConfigKeys(ctx.daemon, defaults.keys(), defaults, [=](const QHash<QString, QString> &values) {
        const int idx = autoShutdown->findData(values.value(QStringLiteral("daemon.server.autoShutdown")));
        if (idx >= 0) {
            autoShutdown->setCurrentIndex(idx);
        }
        idleSeconds->setText(values.value(QStringLiteral("daemon.server.autoShutdownIdleSeconds")));
        logEnabled->setChecked(values.value(QStringLiteral("log.file.enabled")) == QStringLiteral("true"));
        logPath->setText(values.value(QStringLiteral("log.file.path")));
        fileDownload->setChecked(values.value(QStringLiteral("daemon.server.fileDownload.enabled"))
                                 == QStringLiteral("true"));
        fsBrowse->setChecked(values.value(QStringLiteral("daemon.server.fsBrowse.enabled")) == QStringLiteral("true"));
        updateIdleVisible();
    });

    QObject::connect(saveBtn, &QPushButton::clicked, panel, [=, &ctx] {
        QHash<QString, QString> values;
        values.insert(QStringLiteral("daemon.server.autoShutdown"), autoShutdown->currentData().toString());
        values.insert(QStringLiteral("daemon.server.autoShutdownIdleSeconds"), idleSeconds->text());
        values.insert(QStringLiteral("log.file.enabled"), logEnabled->isChecked() ? QStringLiteral("true")
                                                                                 : QStringLiteral("false"));
        values.insert(QStringLiteral("log.file.path"), logPath->text());
        values.insert(QStringLiteral("daemon.server.fileDownload.enabled"),
                      fileDownload->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        values.insert(QStringLiteral("daemon.server.fsBrowse.enabled"),
                      fsBrowse->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        saveBtn->setEnabled(false);
        saveConfigKeys(ctx.daemon, values, [=](bool ok, const QString &err) {
            saveBtn->setEnabled(true);
            status->setText(ok ? ctx.translator.tr(QStringLiteral("settings.saved"))
                               : (err.isEmpty() ? ctx.translator.tr(QStringLiteral("common.error")) : err));
        });
    });

    return wrapScroll(panel);
}

QWidget *buildBrowserSettingsPanel(const SettingsContext &ctx, QWidget *parent)
{
    Translator &translator = ctx.translator;
    AppSettings &appSettings = ctx.appSettings;
    GuiPreferences &guiPreferences = ctx.guiPreferences;
    ExtensionBridgeClient &extension = ctx.extension;
    SessionManager &sessions = ctx.sessions;

    auto *panel = new QWidget(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);

    layout->addWidget(sectionTitle(translator.tr(QStringLiteral("settings.browser.title")), panel));
    layout->addWidget(hintLabel(translator.tr(QStringLiteral("settings.browser.hint")), panel));

    auto *enable = new QCheckBox(translator.tr(QStringLiteral("settings.browser.enableListener")), panel);
    enable->setChecked(appSettings.browserExtensionEnabled());
    QObject::connect(enable, &QCheckBox::toggled, &appSettings, &AppSettings::setBrowserExtensionEnabled);
    layout->addWidget(enable);

    auto *suspendBtn = new AvarButton(AvarButtonVariant::Secondary, panel);
    auto *resumeBtn = new AvarButton(AvarButtonVariant::Primary, panel);
    suspendBtn->setText(translator.tr(QStringLiteral("settings.browser.suspendExtension")));
    resumeBtn->setText(translator.tr(QStringLiteral("settings.browser.resumeExtension")));
    auto *bridgeStatus = new QLabel(panel);
    auto *guiUrl = new QLabel(extension.bridgeBaseUrl() + QStringLiteral("/"), panel);
    guiUrl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *daemonUrl = new QLabel(sessions.activeSession().baseUrl, panel);
    daemonUrl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *copyBtn = new AvarButton(AvarButtonVariant::Secondary, panel);
    copyBtn->setText(translator.tr(QStringLiteral("settings.browser.copyGuiUrl")));
    auto *status = statusLabel(panel);

    auto updateSuspendUi = [=, &appSettings, &guiPreferences, &translator, bridgeStatus, suspendBtn, resumeBtn] {
        const bool suspended = guiPreferences.extensionBridgeSuspended();
        suspendBtn->setVisible(!suspended && appSettings.browserExtensionEnabled());
        resumeBtn->setVisible(suspended && appSettings.browserExtensionEnabled());
        if (!appSettings.browserExtensionEnabled()) {
            bridgeStatus->setText(translator.tr(QStringLiteral("settings.browser.extensionDisabled")));
        } else if (suspended) {
            bridgeStatus->setText(translator.tr(QStringLiteral("settings.browser.extensionSuspended")));
        } else {
            bridgeStatus->setText(translator.tr(QStringLiteral("settings.browser.extensionChecking")));
        }
    };
    updateSuspendUi();
    QObject::connect(&guiPreferences, &GuiPreferences::preferencesChanged, panel, updateSuspendUi);
    QObject::connect(&appSettings, &AppSettings::daemonConfigChanged, panel, updateSuspendUi);
    QObject::connect(&extension, &ExtensionBridgeClient::bridgeReachableChanged, panel,
                     [=, &appSettings, &guiPreferences, &translator, bridgeStatus](bool reachable) {
                         if (!appSettings.browserExtensionEnabled() || guiPreferences.extensionBridgeSuspended()) {
                             return;
                         }
                         bridgeStatus->setText(
                             reachable ? translator.tr(QStringLiteral("settings.browser.extensionConnected"))
                                       : translator.tr(QStringLiteral("settings.browser.extensionDisconnected")));
                     });

    layout->addWidget(suspendBtn);
    layout->addWidget(resumeBtn);
    layout->addWidget(bridgeStatus);
    layout->addWidget(new QLabel(translator.tr(QStringLiteral("settings.browser.guiUrl")), panel));
    layout->addWidget(guiUrl);
    layout->addWidget(copyBtn);
    layout->addWidget(new QLabel(translator.tr(QStringLiteral("settings.browser.daemonUrl")), panel));
    layout->addWidget(daemonUrl);

    QObject::connect(suspendBtn, &QPushButton::clicked, panel, [&guiPreferences, updateSuspendUi] {
        guiPreferences.setExtensionBridgeSuspended(true);
        updateSuspendUi();
    });
    QObject::connect(resumeBtn, &QPushButton::clicked, panel, [&guiPreferences, &extension, updateSuspendUi] {
        guiPreferences.setExtensionBridgeSuspended(false);
        extension.ensureBridgeProcess();
        updateSuspendUi();
    });
    QObject::connect(copyBtn, &QPushButton::clicked, panel, [guiUrl, &translator, status] {
        QGuiApplication::clipboard()->setText(guiUrl->text());
        status->setText(translator.tr(QStringLiteral("settings.browser.guiUrlCopied")));
    });

    layout->addWidget(sectionTitle(translator.tr(QStringLiteral("settings.pwa.notificationsTitle")), panel));
    layout->addWidget(hintLabel(translator.tr(QStringLiteral("settings.pwa.notificationsHint")), panel));
    auto *notifBtn = new AvarButton(AvarButtonVariant::Secondary, panel);
    notifBtn->setText(translator.tr(QStringLiteral("settings.pwa.enableNotifications")));
    QObject::connect(notifBtn, &QPushButton::clicked, panel, [status, &translator] {
        status->setText(translator.tr(QStringLiteral("settings.pwa.notificationsUnsupported")));
    });
    layout->addWidget(notifBtn);
    layout->addWidget(status);
    layout->addStretch();

    ctx.extension.pingBridge();
    return wrapScroll(panel);
}

QWidget *buildShortcutsSettingsPanel(const SettingsContext &ctx, QWidget *parent)
{
    auto *panel = new QWidget(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("shortcuts.hint")), panel));

    auto *table = new QTableWidget(panel);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels(
        {ctx.translator.tr(QStringLiteral("shortcuts.columnAction")), ctx.translator.tr(QStringLiteral("shortcuts.columnKeys"))});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);

    const QVector<ShortcutDefinition> defs = shortcutDefinitions();
    QString lastCategory;
    int row = 0;
    for (const ShortcutDefinition &def : defs) {
        if (def.categoryKey != lastCategory) {
            table->insertRow(row);
            auto *cat = new QTableWidgetItem(ctx.translator.tr(def.categoryKey));
            cat->setFlags(Qt::ItemIsEnabled);
            QFont font = cat->font();
            font.setBold(true);
            cat->setFont(font);
            table->setItem(row, 0, cat);
            table->setSpan(row, 0, 1, 2);
            ++row;
            lastCategory = def.categoryKey;
        }
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(ctx.translator.tr(def.labelKey)));
        auto *comboBtn = new AvarButton(AvarButtonVariant::Secondary);
        comboBtn->setProperty("shortcutId", def.id);
        comboBtn->setText(formatShortcutCombo(ctx.guiPreferences.shortcut(def.id)));
        table->setCellWidget(row, 1, comboBtn);
        QObject::connect(comboBtn, &QPushButton::clicked, panel, [comboBtn, &ctx, def] {
            comboBtn->setText(ctx.translator.tr(QStringLiteral("shortcuts.pressKeys")));
            comboBtn->installEventFilter(new ShortcutCaptureFilter([comboBtn, &ctx, def](const QString &combo) {
                ctx.guiPreferences.setShortcut(def.id, combo);
                comboBtn->setText(formatShortcutCombo(combo));
            }));
            comboBtn->setFocus();
        });
        ++row;
    }
    layout->addWidget(table, 1);

    auto refreshCombos = [table, &ctx] {
        for (int r = 0; r < table->rowCount(); ++r) {
            if (auto *btn = qobject_cast<AvarButton *>(table->cellWidget(r, 1))) {
                const QString id = btn->property("shortcutId").toString();
                if (!id.isEmpty()) {
                    btn->setText(formatShortcutCombo(ctx.guiPreferences.shortcut(id)));
                }
            }
        }
    };
    auto *resetBtn = new AvarButton(AvarButtonVariant::Secondary, panel);
    resetBtn->setText(ctx.translator.tr(QStringLiteral("shortcuts.resetAll")));
    QObject::connect(resetBtn, &QPushButton::clicked, panel, [&ctx, refreshCombos] {
        ctx.guiPreferences.resetShortcutsToDefaults();
        refreshCombos();
    });
    layout->addWidget(resetBtn);

    return wrapScroll(panel);
}

QWidget *buildAboutSettingsPanel(const SettingsContext &ctx, QWidget *parent)
{
    auto *panel = new QWidget(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);

    layout->addWidget(hintLabel(ctx.translator.tr(QStringLiteral("settings.about.intro")), panel));

    auto *versionTable = new QTableWidget(0, 2, panel);
    versionTable->setHorizontalHeaderLabels(
        {ctx.translator.tr(QStringLiteral("settings.about.versionTitle")), QString()});
    versionTable->horizontalHeader()->setVisible(false);
    versionTable->verticalHeader()->setVisible(false);
    versionTable->horizontalHeader()->setStretchLastSection(true);

    auto addRow = [versionTable](const QString &label, const QString &value) {
        const int row = versionTable->rowCount();
        versionTable->insertRow(row);
        versionTable->setItem(row, 0, new QTableWidgetItem(label));
        versionTable->setItem(row, 1, new QTableWidgetItem(value));
    };
    addRow(ctx.translator.tr(QStringLiteral("settings.about.frontendVersion")), QStringLiteral("0.1.0"));
    auto *backendCell = new QTableWidgetItem(ctx.translator.tr(QStringLiteral("settings.about.backendLoading")));
    versionTable->insertRow(1);
    versionTable->setItem(1, 0, new QTableWidgetItem(ctx.translator.tr(QStringLiteral("settings.about.backendVersion"))));
    versionTable->setItem(1, 1, backendCell);
    layout->addWidget(versionTable);

    ctx.daemon.cliExec({QStringLiteral("avar"), QStringLiteral("--version")}, [=](bool ok, const QJsonObject &result,
                                                                                  const QString &) {
        if (!ok) {
            backendCell->setText(ctx.translator.tr(QStringLiteral("settings.about.backendUnknown")));
            return;
        }
        const QString output = result.value(QStringLiteral("output")).toString().trimmed();
        static const QRegularExpression re(QStringLiteral("Avar version:\\s*(\\S+)"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = re.match(output);
        backendCell->setText(match.hasMatch() ? match.captured(1)
                                              : (output.isEmpty() ? ctx.translator.tr(QStringLiteral("settings.about.backendUnknown"))
                                                                  : output));
    });

    const QString repo = QStringLiteral("https://github.com/amkherad/avar");
    auto addLinkSection = [&](const QString &headingKey, const QString &textKey, const QString &buttonKey,
                              const QString &url, bool primary) {
        layout->addWidget(sectionTitle(ctx.translator.tr(headingKey), panel));
        layout->addWidget(hintLabel(ctx.translator.tr(textKey), panel));
        auto *btn = new AvarButton(primary ? AvarButtonVariant::Primary : AvarButtonVariant::Secondary, panel);
        btn->setText(ctx.translator.tr(buttonKey));
        QObject::connect(btn, &QPushButton::clicked, panel, [url] { QDesktopServices::openUrl(QUrl(url)); });
        layout->addWidget(btn);
    };

    addLinkSection(QStringLiteral("settings.about.authorTitle"), QStringLiteral("settings.about.authorText"),
                   QStringLiteral("settings.about.authorButton"), QStringLiteral("https://github.com/amkherad"), false);
    addLinkSection(QStringLiteral("settings.about.licenseTitle"), QStringLiteral("settings.about.licenseText"),
                   QStringLiteral("settings.about.licenseButton"), repo + QStringLiteral("#license"), false);
    addLinkSection(QStringLiteral("settings.about.sponsorsTitle"), QStringLiteral("settings.about.sponsorsText"),
                   QStringLiteral("settings.about.sponsorsButton"), QStringLiteral("https://github.com/sponsors/amkherad"),
                   true);
    addLinkSection(QStringLiteral("settings.about.reportBugTitle"), QStringLiteral("settings.about.reportBugText"),
                   QStringLiteral("settings.about.reportBugButton"), repo + QStringLiteral("/issues/new"), false);

    layout->addStretch();
    return wrapScroll(panel);
}

} // namespace avar::gui
