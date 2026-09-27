#pragma once

#include "api/DaemonTypes.hpp"
#include "settings/SettingsCategory.hpp"

#include <QWidget>

class QListWidget;

namespace avar::gui {

class ThemeManager;
class Translator;

class QueuePanelWidget final : public QWidget {
    Q_OBJECT

public:
    QueuePanelWidget(Translator &translator, ThemeManager &theme, QWidget *parent = nullptr);

    void setQueues(const QVector<QueueInfo> &queues);
    [[nodiscard]] QString selectedQueueId() const;

    void retranslateUi();

signals:
    void queueSelected(const QString &queueId);
    void addQueueRequested();
    void openQueueSettingsRequested();

private:
    void refreshIcons();

    Translator &m_tr;
    ThemeManager &m_theme;
    QListWidget *m_list = nullptr;
};

} // namespace avar::gui
