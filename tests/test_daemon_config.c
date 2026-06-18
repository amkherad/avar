#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include <daemon/daemon.h>
#include "config.h"
#include "file-system.h"
#include "logger.h"

static TestGuard g_guard;

static void setup_daemon_paths(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-daemon-cfg"));
    remove(g_guard.config_path);
    init_logger(false);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
}

AVAR_TEST(daemon_config_defaults_and_apply) {
    setup_daemon_paths();

    DaemonConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    daemon_config_apply_defaults(&cfg);
    AVAR_ASSERT(cfg.session.mode == AvarSessionModeLocal);

    DaemonStartOptions opts = {.http_override = true, .http_port = "19090"};
    daemon_config_apply_start_options(&cfg, &opts);
    AVAR_ASSERT(cfg.server.http.enabled);
}

AVAR_TEST(daemon_pid_file_roundtrip) {
    setup_daemon_paths();

    char pid_path[512];
    snprintf(pid_path, sizeof pid_path, "%s%cdaemon.pid", g_guard.work_dir, PATH_SEPARATOR);
    remove(pid_path);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SERVER_PID_FILE, pid_path), 0);

    AVAR_ASSERT_EQ(daemon_write_pid_file(pid_path), EXIT_SUCCESS);
    AVAR_ASSERT(file_exists(pid_path));

    int pid = 0;
    AVAR_ASSERT(daemon_is_running(&pid));

    AVAR_ASSERT_EQ(daemon_remove_pid_file(pid_path), EXIT_SUCCESS);
    AVAR_ASSERT(!file_exists(pid_path));
    AVAR_ASSERT(daemon_cleanup_stale_pid_file(pid_path));
}

AVAR_TEST(daemon_config_load_from_test_config) {
    setup_daemon_paths();

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_LOCAL), 0);
    DaemonConfig cfg;
    AVAR_ASSERT(daemon_config_load(&cfg));
    AVAR_ASSERT_EQ(cfg.session.mode, AvarSessionModeLocal);
}

AVAR_TEST_MAIN(
        run_daemon_config_defaults_and_apply();
        run_daemon_pid_file_roundtrip();
        run_daemon_config_load_from_test_config();)
