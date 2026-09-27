#include "core/CrashHandler.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

#if !defined(AVAR_GUI_HOSTING_WASM) && (defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID))
#include <csignal>
#include <unistd.h>
#endif

namespace avar::gui {

namespace {

QMutex g_logMutex;
QtMessageHandler g_previousHandler = nullptr;

void appendCrashLine(const QString &line)
{
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/crash.log");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << ' ' << line << '\n';
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker lock(&g_logMutex);
    if (g_previousHandler) {
        g_previousHandler(type, context, msg);
    }
    if (type == QtFatalMsg) {
        appendCrashLine(QStringLiteral("FATAL: %1").arg(msg));
    }
}

#if !defined(AVAR_GUI_HOSTING_WASM) && (defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID))
void onFatalSignal(int sig)
{
    const char *name = "SIGNAL";
    switch (sig) {
    case SIGSEGV:
        name = "SIGSEGV";
        break;
    case SIGABRT:
        name = "SIGABRT";
        break;
    case SIGFPE:
        name = "SIGFPE";
        break;
    case SIGILL:
        name = "SIGILL";
        break;
    default:
        break;
    }
    appendCrashLine(QStringLiteral("Caught %1").arg(QString::fromUtf8(name)));
    _exit(128 + sig);
}
#endif

} // namespace

void installCrashHandler()
{
    g_previousHandler = qInstallMessageHandler(messageHandler);

#if !defined(AVAR_GUI_HOSTING_WASM) && (defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID))
    struct sigaction action {};
    action.sa_handler = onFatalSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGABRT, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
#endif
}

} // namespace avar::gui
