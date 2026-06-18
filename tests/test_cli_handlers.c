#include "avar_test.h"
#include "test_guard.h"

#include <cli.h>
#include <daemon/daemon_session.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "download.h"
#include "file-system.h"
#include "logger.h"
#include "queue.h"

#ifndef AVAR_SOURCE_DIR
#define AVAR_SOURCE_DIR "."
#endif

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

static TestGuard g_guard;
static char g_server_script[512];

#if defined(_WIN32)
static PROCESS_INFORMATION g_server_proc = {0};
#else
static pid_t g_server_pid;
#endif

static void start_local_server(void) {
    snprintf(g_server_script, sizeof g_server_script, "%s/scripts/http_test_server.py",
             AVAR_SOURCE_DIR);
    test_guard_set_server_env(&g_guard);

#if defined(_WIN32)
    if (g_server_proc.hProcess != NULL) {
        TerminateProcess(g_server_proc.hProcess, 0);
        CloseHandle(g_server_proc.hProcess);
        CloseHandle(g_server_proc.hThread);
        memset(&g_server_proc, 0, sizeof g_server_proc);
    }

    STARTUPINFOA si = {0};
    si.cb = sizeof si;
    char command[768];
    snprintf(command, sizeof command, "python \"%s\"", g_server_script);
    AVAR_ASSERT(CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si,
                               &g_server_proc));
    Sleep(1000);
#else
    g_server_pid = fork();
    AVAR_ASSERT(g_server_pid != -1);
    if (g_server_pid == 0) {
        execlp("python3", "python3", g_server_script, NULL);
        execlp("python", "python", g_server_script, NULL);
        _exit(127);
    }
    usleep(500000);
#endif
}

static void stop_local_server(void) {
#if defined(_WIN32)
    if (g_server_proc.hProcess != NULL) {
        TerminateProcess(g_server_proc.hProcess, 0);
        WaitForSingleObject(g_server_proc.hProcess, 2000);
        CloseHandle(g_server_proc.hProcess);
        CloseHandle(g_server_proc.hThread);
        memset(&g_server_proc, 0, sizeof g_server_proc);
    }
#else
    if (g_server_pid > 0) {
        kill(g_server_pid, SIGTERM);
        waitpid(g_server_pid, NULL, 0);
        g_server_pid = 0;
    }
#endif
}

static void setup_local_cli(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-cli-handlers"));
    remove(g_guard.config_path);
    init_logger(false);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_LOCAL), 0);
    daemon_session_deinit();
    AVAR_ASSERT_EQ(daemon_session_init(), DaemonSessionErrorNone);
    daemon_session_force_local(true);
}

AVAR_TEST(cli_handle_config_get_set_reset) {
    setup_local_cli();

    char *argv_set[] = {"avar", "config", "set", "download.test.key", "cli-value"};
    AVAR_ASSERT_EQ(handle_config(5, argv_set), EXIT_SUCCESS);

    char *argv_get[] = {"avar", "config", "get", "download.test.key"};
    AVAR_ASSERT_EQ(handle_config(4, argv_get), EXIT_SUCCESS);

    char *argv_get_json[] = {"avar", "config", "get", "download.test.key", "--format=json"};
    AVAR_ASSERT_EQ(handle_config(5, argv_get_json), EXIT_SUCCESS);

    char *value = get_config("download.test.key");
    AVAR_ASSERT_NOT_NULL(value);
    AVAR_ASSERT_STR_EQ(value, "cli-value");
    free(value);

    char save_path[512];
    snprintf(save_path, sizeof save_path, "%s%cexported.json", g_guard.work_dir, PATH_SEPARATOR);
    char *argv_save[] = {"avar", "config", "save", save_path};
    AVAR_ASSERT_EQ(handle_config(4, argv_save), EXIT_SUCCESS);

    char *argv_reset[] = {"avar", "config", "reset", "download.test.key"};
    AVAR_ASSERT_EQ(handle_config(4, argv_reset), EXIT_SUCCESS);
    AVAR_ASSERT_NULL(get_config("download.test.key"));

    char *argv_load[] = {"avar", "config", "load", save_path};
    AVAR_ASSERT_EQ(handle_config(4, argv_load), EXIT_SUCCESS);
    value = get_config("download.test.key");
    AVAR_ASSERT_NOT_NULL(value);
    AVAR_ASSERT_STR_EQ(value, "cli-value");
    free(value);
}

AVAR_TEST(cli_handle_queue_lifecycle) {
    setup_local_cli();

    char *argv_add[] = {"avar", "queue", "add", "cli-queue", "--description=from-cli",
                        "--maxConcurrentDownloads=2", "--maxConnections=4"};
    AVAR_ASSERT_EQ(handle_queue(7, argv_add), EXIT_SUCCESS);
    AVAR_ASSERT_EQ(queue_count(), 1U);

    char *argv_ls[] = {"avar", "queue", "ls"};
    AVAR_ASSERT_EQ(handle_queue(3, argv_ls), EXIT_SUCCESS);

    char *queue_id = queue_resolve_id("cli-queue", true);
    AVAR_ASSERT_NOT_NULL(queue_id);

    char edit_cmd[256];
    snprintf(edit_cmd, sizeof edit_cmd, "%s", queue_id);
    char *argv_edit[] = {"avar", "queue", "edit", edit_cmd, "--description=edited"};
    AVAR_ASSERT_EQ(handle_queue(5, argv_edit), EXIT_SUCCESS);

    char *argv_stop[] = {"avar", "queue", "stop", queue_id};
    AVAR_ASSERT_EQ(handle_queue(4, argv_stop), EXIT_SUCCESS);

    char *argv_start[] = {"avar", "queue", "start", queue_id};
    AVAR_ASSERT_EQ(handle_queue(4, argv_start), EXIT_SUCCESS);

    char *argv_rm[] = {"avar", "queue", "rm", queue_id};
    AVAR_ASSERT_EQ(handle_queue(4, argv_rm), EXIT_SUCCESS);
    free(queue_id);
    AVAR_ASSERT_EQ(queue_count(), 0U);
}

AVAR_TEST(cli_handle_download_subcommands) {
    setup_local_cli();

    char *argv_add[] = {"avar", "download", "add", "sample-dl"};
    AVAR_ASSERT_EQ(handle_download(4, argv_add), EXIT_SUCCESS);

    char *argv_ls[] = {"avar", "download", "ls"};
    AVAR_ASSERT_EQ(handle_download(3, argv_ls), EXIT_SUCCESS);
}

AVAR_TEST(cli_handle_profile_and_scheduler) {
    setup_local_cli();

    char *argv_profile_add[] = {"avar", "profile", "add", "work"};
    AVAR_ASSERT_EQ(handle_profile(4, argv_profile_add), EXIT_SUCCESS);

    char *argv_profile_ls[] = {"avar", "profile", "ls"};
    AVAR_ASSERT_EQ(handle_profile(3, argv_profile_ls), EXIT_SUCCESS);

    char *argv_profile_rm[] = {"avar", "profile", "rm", "work", "--force"};
    AVAR_ASSERT_EQ(handle_profile(5, argv_profile_rm), EXIT_SUCCESS);

    char *argv_sched_add[] = {"avar", "scheduler", "add", "nightly"};
    AVAR_ASSERT_EQ(handle_scheduler(4, argv_sched_add), EXIT_SUCCESS);

    char *argv_sched_ls[] = {"avar", "scheduler", "ls"};
    AVAR_ASSERT_EQ(handle_scheduler(3, argv_sched_ls), EXIT_SUCCESS);

    char *argv_sched_rm[] = {"avar", "scheduler", "rm", "nightly"};
    AVAR_ASSERT_EQ(handle_scheduler(4, argv_sched_rm), EXIT_SUCCESS);
}

AVAR_TEST(cli_handle_daemon_status_and_ping) {
    setup_local_cli();

    char *argv_status[] = {"avar", "daemon", "status"};
    AVAR_ASSERT_EQ(handle_daemon(3, argv_status), EXIT_SUCCESS);

    char *argv_ping[] = {"avar", "daemon", "ping"};
    (void)handle_daemon(3, argv_ping);

    char *argv_stop[] = {"avar", "daemon", "stop"};
    (void)handle_daemon(3, argv_stop);

    char *argv_reload[] = {"avar", "daemon", "reload"};
    (void)handle_daemon(3, argv_reload);

    char *argv_logs_help[] = {"avar", "daemon", "logs", "--help"};
    AVAR_ASSERT_EQ(handle_daemon(4, argv_logs_help), EXIT_SUCCESS);

    char *argv_unknown[] = {"avar", "daemon", "not-a-command"};
    AVAR_ASSERT_EQ(handle_daemon(3, argv_unknown), EXIT_UNKNOWN_COMMAND);
}

AVAR_TEST(cli_handle_download_url_attached) {
    setup_local_cli();

    char temp_dir[512];
    char download_dir[512];
    snprintf(temp_dir, sizeof temp_dir, "%s%ctemp", g_guard.work_dir, PATH_SEPARATOR);
    snprintf(download_dir, sizeof download_dir, "%s%cdownloads", g_guard.work_dir, PATH_SEPARATOR);
    (void)make_dirs_in_path(temp_dir);
    (void)make_dirs_in_path(download_dir);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_TEMP_PATH, temp_dir), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_DOWNLOAD_PATH, download_dir), 0);

    start_local_server();

    char url[256];
    AVAR_ASSERT(test_guard_http_url(&g_guard, "plain.txt", url, sizeof url));

    char *argv_dl[] = {"avar", "download", url, "--attached"};
    AVAR_ASSERT_EQ(handle_download(4, argv_dl), EXIT_SUCCESS);

    stop_local_server();
}

AVAR_TEST(cli_handle_config_errors) {
    setup_local_cli();

    char *argv_get_missing[] = {"avar", "config", "get", "missing.key.xyz"};
    AVAR_ASSERT_EQ(handle_config(4, argv_get_missing), EXIT_FAILURE);

    char *argv_get_default[] = {"avar", "config", "get", "missing.key.xyz",
                                "--defaultValue=fallback"};
    AVAR_ASSERT_EQ(handle_config(5, argv_get_default), EXIT_SUCCESS);
}

AVAR_TEST(cli_handle_download_and_daemon_help) {
    setup_local_cli();

    char *argv_dl_help[] = {"avar", "download", "--help"};
    AVAR_ASSERT_EQ(handle_download(3, argv_dl_help), EXIT_SUCCESS);

    char *argv_daemon_help[] = {"avar", "daemon", "--help"};
    AVAR_ASSERT_EQ(handle_daemon(3, argv_daemon_help), EXIT_SUCCESS);

    char *argv_profile_help[] = {"avar", "profile", "--help"};
    AVAR_ASSERT_EQ(handle_profile(3, argv_profile_help), EXIT_SUCCESS);

    char *argv_scheduler_help[] = {"avar", "scheduler", "--help"};
    AVAR_ASSERT_EQ(handle_scheduler(3, argv_scheduler_help), EXIT_SUCCESS);
}

AVAR_TEST(cli_dispatch_routes_commands) {
    setup_local_cli();

    char *argv[] = {"avar", "config", "reset-all"};
    AVAR_ASSERT_EQ(cli_run(3, argv), EXIT_SUCCESS);
}

AVAR_TEST_MAIN(
        run_cli_handle_config_get_set_reset();
        run_cli_handle_queue_lifecycle();
        run_cli_handle_download_subcommands();
        run_cli_handle_download_url_attached();
        run_cli_handle_profile_and_scheduler();
        run_cli_handle_daemon_status_and_ping();
        run_cli_handle_config_errors();
        run_cli_handle_download_and_daemon_help();
        run_cli_dispatch_routes_commands();)
