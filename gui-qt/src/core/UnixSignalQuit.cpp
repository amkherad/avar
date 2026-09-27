#include "core/UnixSignalQuit.hpp"

#if defined(AVAR_GUI_HOSTING_DESKTOP) && !defined(_WIN32)

#include <QCoreApplication>
#include <QSocketNotifier>

#include <csignal>
#include <unistd.h>

namespace avar::gui {

UnixSignalQuit *UnixSignalQuit::s_instance = nullptr;

namespace {

void signalWriteHandler(int)
{
    UnixSignalQuit::notifyFromSignal();
}

} // namespace

UnixSignalQuit::UnixSignalQuit(QCoreApplication &app)
    : m_app(app)
{
    int fds[2] = {-1, -1};
    if (pipe(fds) != 0) {
        return;
    }
    m_readFd = fds[0];
    m_writeFd = fds[1];

    struct sigaction action {};
    action.sa_handler = signalWriteHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);

    m_notifier = new QSocketNotifier(m_readFd, QSocketNotifier::Read, &m_app);
    QObject::connect(m_notifier, &QSocketNotifier::activated, &m_app, [this] {
        char buffer[32];
        while (read(m_readFd, buffer, sizeof buffer) > 0) {
        }
        m_app.quit();
    });

    s_instance = this;
}

UnixSignalQuit::~UnixSignalQuit()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
    if (m_notifier != nullptr) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_readFd >= 0) {
        close(m_readFd);
        m_readFd = -1;
    }
    if (m_writeFd >= 0) {
        close(m_writeFd);
        m_writeFd = -1;
    }
}

void UnixSignalQuit::notifyFromSignal()
{
    if (s_instance == nullptr || s_instance->m_writeFd < 0) {
        return;
    }
    const char byte = 1;
    (void)write(s_instance->m_writeFd, &byte, 1);
}

} // namespace avar::gui

#else

namespace avar::gui {

UnixSignalQuit *UnixSignalQuit::s_instance = nullptr;

UnixSignalQuit::UnixSignalQuit(QCoreApplication &)
{
}

UnixSignalQuit::~UnixSignalQuit() = default;

void UnixSignalQuit::notifyFromSignal() {}

} // namespace avar::gui

#endif
