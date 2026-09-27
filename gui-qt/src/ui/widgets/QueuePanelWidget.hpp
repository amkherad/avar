#pragma once

#include "api/DaemonTypes.hpp"
#include "settings/SettingsCategory.hpp"

#include <QWidget>

class QListWidget;

namespace avar::gui {

class Translator;

class QueuePanelWidget final : public QWidget {
    Q_OBJECT

public:
    QueuePanelWidget(Translator &translator, QWidget *parent = nullptr);

    void setQueues(const QVector<QueueInfo> &queues);
    [[nodiscard]] QString selectedQueueId() const;

signals:
    void queueSelected(const QString &queueId);
    void addQueueRequested();
    void openQueueSettingsRequested();

private:
    Translator &m_tr;
    QListWidget *m_list = nullptr;
};

} // namespace avar::gui
