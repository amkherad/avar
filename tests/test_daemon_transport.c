#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include <daemon/daemon.h>
#include <daemon/daemon_rpc.h>
#include <daemon/daemon_transport.h>
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

static void setup_transport_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-transport"));
    remove(g_guard.config_path);

#if defined(_WIN32)
    snprintf(g_pipe_path, sizeof g_pipe_path, "\\\\.\\pipe\\%s", g_guard.instance_id);
#else
    snprintf(g_pipe_path, sizeof g_pipe_path, "/tmp/%s.pipe", g_guard.instance_id);
    remove(g_pipe_path);
#endif

    snprintf(g_pid_path, sizeof g_pid_path, "%s%cdaemon.pid", g_guard.work_dir, PATH_SEPARATOR);
    remove(g_pid_path);

    init_logger(false);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_LOCAL), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_CHANNELS_HTTP_ENABLED, "true"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_CHANNELS_HTTP_BIND, "127.0.0.1"), 0);

    char http_port[16];
    snprintf(http_port, sizeof http_port, "%d", g_guard.http_port);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_CHANNELS_HTTP_PORT, http_port), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_CHANNELS_PIPE_ENABLED, "true"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_CHANNELS_PIPE_NAME, g_pipe_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SERVER_PID_FILE, g_pid_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SERVER_DETACH, "false"), 0);
}

static bool load_cfg(DaemonConfig *cfg) {
    return cfg != NULL && daemon_config_load(cfg);
}

AVAR_TEST(transport_create_destroy_kinds) {
    setup_transport_config();

    DaemonTransport *http = daemon_transport_create(AvarTransportHttp);
    AVAR_ASSERT_NOT_NULL(http);
    daemon_transport_destroy(http);

    DaemonTransport *pipe = daemon_transport_create(AvarTransportPipe);
    AVAR_ASSERT_NOT_NULL(pipe);
    daemon_transport_destroy(pipe);

    DaemonTransport *unix_transport = daemon_transport_create(AvarTransportUnix);
    AVAR_ASSERT_NOT_NULL(unix_transport);
    daemon_transport_destroy(unix_transport);

    DaemonTransport *https = daemon_transport_create_https();
    AVAR_ASSERT_NOT_NULL(https);
    daemon_transport_destroy(https);
}

AVAR_TEST(transport_ping_null_and_disabled) {
    setup_transport_config();

    AVAR_ASSERT(!daemon_transport_ping_any(NULL));
    AVAR_ASSERT(!daemon_transport_ping_any_timeout(NULL, 100U));
    AVAR_ASSERT(!daemon_transport_ping_remote(AvarTransportHttp, NULL));

    DaemonConfig cfg;
    AVAR_ASSERT(load_cfg(&cfg));
    cfg.server.http.enabled = false;
    cfg.server.pipe.enabled = false;
    cfg.server.unix_socket.enabled = false;
    AVAR_ASSERT(!daemon_transport_ping_any(&cfg));
}

AVAR_TEST(transport_http_start_poll_stop) {
    setup_transport_config();
    daemon_rpc_init();

    DaemonConfig cfg;
    AVAR_ASSERT(load_cfg(&cfg));
    DaemonTransport *http = daemon_transport_create(AvarTransportHttp);
    AVAR_ASSERT_NOT_NULL(http);
    AVAR_ASSERT(daemon_transport_start(http, &cfg));
    daemon_transport_poll(http, 50U);
    AVAR_ASSERT(daemon_transport_ping(http));
    daemon_transport_stop(http);
    daemon_transport_destroy(http);
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

AVAR_TEST(transport_pipe_rpc_while_daemon_running) {
    setup_transport_config();
    daemon_rpc_init();

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_REMOTE), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_TRANSPORT, AVAR_DAEMON_TRANSPORT_PIPE), 0);

    DaemonConfig cfg;
    AVAR_ASSERT(load_cfg(&cfg));

#if defined(_WIN32)
    HANDLE thread = CreateThread(NULL, 0, daemon_thread_main, (LPVOID)&cfg, 0, NULL);
    AVAR_ASSERT_NOT_NULL(thread);
#else
    pthread_t thread;
    AVAR_ASSERT_EQ(pthread_create(&thread, NULL, daemon_thread_main, &cfg), 0);
#endif

    bool ready = false;
    for (int i = 0; i < 50; ++i) {
        if (daemon_transport_ping_any_timeout(&cfg, 100U)) {
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

    char *response = NULL;
    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}";
    AVAR_ASSERT(daemon_transport_rpc_request(AvarTransportPipe, &cfg, req, &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "ok") != NULL);
    free(response);

    AVAR_ASSERT(daemon_transport_rpc_request_any(&cfg, req, &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);

    AVAR_ASSERT(daemon_transport_ping_any(&cfg));
    AVAR_ASSERT(daemon_transport_ping_any_timeout(&cfg, 1000U));

    DaemonStopOptions opts = {.wait = true, .force_kill = false};
    AVAR_ASSERT_EQ(daemon_stop(&opts), EXIT_SUCCESS);

#if defined(_WIN32)
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
}

AVAR_TEST_MAIN(
        run_transport_create_destroy_kinds();
        run_transport_ping_null_and_disabled();
        run_transport_http_start_poll_stop();
        run_transport_pipe_rpc_while_daemon_running();)
