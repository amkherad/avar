#pragma once

#include <QObject>

#include <atomic>
#include <thread>

#include <daemon/daemon.h>

namespace avar::gui {

class EmbeddedDaemon final : public QObject {
    Q_OBJECT

public:
    explicit EmbeddedDaemon(QObject *parent = nullptr);
    ~EmbeddedDaemon() override;

    void start();
    void stop();
    [[nodiscard]] bool isRunning() const;

signals:
    void runningChanged(bool running);

private:
    void runDaemonLoop();
    void setRunning(bool running);

    DaemonConfig *m_config = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
};

} // namespace avar::gui
