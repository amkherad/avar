#include "backend/EmbeddedDaemon.hpp"

#include "core/UnixSignalQuit.hpp"

#include <daemon/daemon.h>
#include <daemon/daemon_embed.h>

#include <QDir>
#include <QStandardPaths>

#include <chrono>
#include <cstring>
#include <thread>

namespace avar::gui {

EmbeddedDaemon::EmbeddedDaemon(QObject *parent)
    : QObject(parent)
{
}

EmbeddedDaemon::~EmbeddedDaemon()
{
    stop();
}

void EmbeddedDaemon::setRunning(bool running)
{
    const bool was = m_running.exchange(running);
    if (was != running) {
        emit runningChanged(running);
    }
}

void EmbeddedDaemon::start()
{
    if (m_running.load() || m_thread.joinable()) {
        return;
    }

    m_stopRequested.store(false);
    delete m_config;
    m_config = new DaemonConfig{};
    daemon_config_load(m_config);

    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
    const QString pidPath = cacheDir + QStringLiteral("/embedded-daemon.pid");
    daemon_embed_apply_gui_defaults(m_config, pidPath.toUtf8().constData());
    daemon_embed_set_ctrl_c_notify(&UnixSignalQuit::notifyFromSignal);

    m_thread = std::thread([this] { runDaemonLoop(); });
    for (int i = 0; i < 200 && !daemon_loop_is_running(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    setRunning(daemon_loop_is_running());
}

void EmbeddedDaemon::stop()
{
    if (!m_thread.joinable()) {
        setRunning(false);
        return;
    }

    m_stopRequested.store(true);
    daemon_embed_set_ctrl_c_notify(nullptr);
    daemon_request_shutdown();
    m_thread.join();
    delete m_config;
    m_config = nullptr;
    setRunning(false);
}

bool EmbeddedDaemon::isRunning() const
{
    return m_running.load();
}

void EmbeddedDaemon::runDaemonLoop()
{
    if (m_config == nullptr) {
        return;
    }
    (void)daemon_start(m_config);
    setRunning(false);
}

} // namespace avar::gui
