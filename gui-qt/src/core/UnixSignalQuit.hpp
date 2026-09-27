#pragma once

class QCoreApplication;
class QSocketNotifier;

namespace avar::gui {

/** Wakes the Qt event loop on SIGINT/SIGTERM (async-signal-safe notify). */
class UnixSignalQuit final {
public:
    explicit UnixSignalQuit(QCoreApplication &app);
    ~UnixSignalQuit();

    static void notifyFromSignal();

private:
    static UnixSignalQuit *s_instance;

    QCoreApplication &m_app;
    int m_readFd = -1;
    int m_writeFd = -1;
    QSocketNotifier *m_notifier = nullptr;
};

} // namespace avar::gui
