#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include <daemon/daemon.h>
#include <daemon/daemon_transport.h>
#include <daemon/daemon_rpc.h>
#include "file-system.h"
#include "logger.h"

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

static TestGuard g_guard;
static char g_pid_path[512];
static char g_pipe_path[512];

static void setup_daemon_test_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-daemon"));

#if defined(_WIN32)
    snprintf(g_pipe_path, sizeof g_pipe_path, "\\\\.\\pipe\\%s", g_guard.instance_id);
#else
    snprintf(g_pipe_path, sizeof g_pipe_path, "/tmp/%s.pipe", g_guard.instance_id);
#endif

    snprintf(g_pid_path, sizeof g_pid_path, "%s%cdaemon.pid", g_guard.work_dir, PATH_SEPARATOR);

    remove(g_guard.config_path);
    remove(g_pid_path);
#if !defined(_WIN32)
    remove(g_pipe_path);
#endif

    init_logger(false);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_REMOTE), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_TRANSPORT, AVAR_DAEMON_TRANSPORT_PIPE), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_CHANNELS_HTTP_ENABLED, "false"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_CHANNELS_PIPE_ENABLED, "true"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_CHANNELS_PIPE_NAME, g_pipe_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SERVER_PID_FILE, g_pid_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SERVER_DETACH, "false"), 0);
}

#if defined(_WIN32)
static DWORD WINAPI daemon_thread_main(LPVOID arg) {
    daemon_start((const DaemonConfig *)arg);
    return 0;
}
#else
static void *daemon_thread_main(void *arg) {
    daemon_start((const DaemonConfig *)arg);
    return NULL;
}
#endif

AVAR_TEST(daemon_start_ping_stop) {
    setup_daemon_test_config();

    DaemonConfig cfg;
    AVAR_ASSERT(daemon_config_load(&cfg));

    bool ping_before = daemon_transport_ping_remote(AvarTransportPipe, &cfg);
    AVAR_ASSERT(!ping_before);

#if defined(_WIN32)
    HANDLE thread = CreateThread(NULL, 0, daemon_thread_main, (LPVOID)&cfg, 0, NULL);
    AVAR_ASSERT_NOT_NULL(thread);
#else
    pthread_t thread;
    AVAR_ASSERT_EQ(pthread_create(&thread, NULL, daemon_thread_main, &cfg), 0);
#endif

    bool ready = false;
    for (int i = 0; i < 50; ++i) {
        if (daemon_transport_ping_remote(AvarTransportPipe, &cfg)) {
            ready = true;
            break;
        }
#if defined(_WIN32)
        Sleep(100);
#else
        usleep(100000);
#endif
    }
    AVAR_ASSERT(ready);

    DaemonStopOptions opts = {.wait = true, .force_kill = false};
    AVAR_ASSERT_EQ(daemon_stop(&opts), EXIT_SUCCESS);

#if defined(_WIN32)
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif

    remove(g_guard.config_path);
    remove(g_pid_path);
#if !defined(_WIN32)
    remove(g_pipe_path);
#endif
}

AVAR_TEST(daemon_rpc_frontend_client_activity) {
    daemon_rpc_init();
    AVAR_ASSERT(!daemon_rpc_frontend_clients_active(60U));
    daemon_rpc_note_frontend_activity();
    AVAR_ASSERT(daemon_rpc_frontend_clients_active(60U));
}

AVAR_TEST_MAIN(
    run_daemon_rpc_frontend_client_activity();
    run_daemon_start_ping_stop();
)
