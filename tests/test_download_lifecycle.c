#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "download.h"
#include "file-system.h"
#include "thread_pool.h"

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
static char g_temp_dir[512];
static char g_download_dir[512];
static char g_server_script[512];

#if defined(_WIN32)
static PROCESS_INFORMATION g_server_proc = {0};
#else
static pid_t g_server_pid;
#endif

static void setup_paths(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-dl-lifecycle"));
    test_guard_set_server_env(&g_guard);

    snprintf(g_temp_dir, sizeof g_temp_dir, "%s%ctemp", g_guard.work_dir, PATH_SEPARATOR);
    snprintf(g_download_dir, sizeof g_download_dir, "%s%cdownload", g_guard.work_dir,
             PATH_SEPARATOR);
    snprintf(g_server_script, sizeof g_server_script, "%s/scripts/http_test_server.py",
             AVAR_SOURCE_DIR);

    (void)make_dirs_in_path(g_temp_dir);
    (void)make_dirs_in_path(g_download_dir);

    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_TEMP_PATH, g_temp_dir), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_DOWNLOAD_PATH, g_download_dir), 0);
}

static void start_local_server(void) {
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

static void build_url(const char *path, char *out, size_t out_size) {
    AVAR_ASSERT(test_guard_http_url(&g_guard, path, out, out_size));
}

AVAR_TEST(download_lifecycle_enqueue_and_remove) {
    setup_paths();
    start_local_server();

    char url[256];
    build_url("plain.txt", url, sizeof url);

    AVAR_ASSERT_EQ(transient_download(url, NULL, "lifecycle-test", NULL, true), EXIT_SUCCESS);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 1);

    char *item_id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_ID);
    AVAR_ASSERT_NOT_NULL(item_id);

    stop_local_server();

    char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_STATUS);
    AVAR_ASSERT_NOT_NULL(status);
    AVAR_ASSERT_STR_EQ(status, AVAR_DL_STATUS_COMPLETED);
    free(status);

    AVAR_ASSERT_EQ(download_remove(item_id, true, true, false), EXIT_SUCCESS);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 0);

    char *dest = path_join(g_download_dir, "plain.txt");
    AVAR_ASSERT_NOT_NULL(dest);
    AVAR_ASSERT(!file_exists(dest));
    free(dest);
    free(item_id);
}

AVAR_TEST(download_lifecycle_pause_resume_background) {
    setup_paths();
    thread_pool_reset_global();
    start_local_server();

    char url[256];
    build_url("segmented.bin", url, sizeof url);

    char *item_id = NULL;
    AVAR_ASSERT_EQ(download_start_background(url, NULL, "pause-test", &item_id), EXIT_SUCCESS);
    AVAR_ASSERT_NOT_NULL(item_id);

    for (int i = 0; i < 50; i++) {
        if (download_pause(item_id) == EXIT_SUCCESS) {
            (void)download_resume(item_id);
            break;
        }
#if defined(_WIN32)
        Sleep(20);
#else
        usleep(20000);
#endif
    }

    AVAR_ASSERT(download_wait_idle(60000U));
    stop_local_server();
    thread_pool_reset_global();

    AVAR_ASSERT_EQ(download_remove(item_id, true, true, false), EXIT_SUCCESS);
    free(item_id);
}

AVAR_TEST(download_lifecycle_enqueue_proxy_and_active_list) {
    setup_paths();
    thread_pool_reset_global();
    start_local_server();

    char url[256];
    build_url("plain.txt", url, sizeof url);

    char *item_id = NULL;
    AVAR_ASSERT_EQ(download_enqueue_with_proxy(url, NULL, "enqueue-test", NULL, &item_id),
                   EXIT_SUCCESS);
    AVAR_ASSERT_NOT_NULL(item_id);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 1);

    AVAR_ASSERT_EQ(download_start(item_id), EXIT_SUCCESS);
    AVAR_ASSERT(download_wait_idle(60000U));

    DownloadActiveInfo active[4];
    const size_t active_count = download_active_list(active, 4U);
    (void)active_count;

    stop_local_server();
    thread_pool_reset_global();

    AVAR_ASSERT_EQ(download_remove(item_id, true, true, false), EXIT_SUCCESS);
    free(item_id);
}

AVAR_TEST(download_lifecycle_start_stop_background) {
    setup_paths();
    thread_pool_reset_global();
    start_local_server();

    char url[256];
    build_url("plain.txt", url, sizeof url);

    char *item_id = NULL;
    AVAR_ASSERT_EQ(download_start_background_with_proxy(url, NULL, "stop-test", NULL, &item_id),
                   EXIT_SUCCESS);
    AVAR_ASSERT_NOT_NULL(item_id);

    for (int i = 0; i < 20; i++) {
        if (download_stop(item_id) == EXIT_SUCCESS) {
            break;
        }
#if defined(_WIN32)
        Sleep(50);
#else
        usleep(50000);
#endif
    }

    AVAR_ASSERT(download_wait_idle(60000U));
    stop_local_server();
    thread_pool_reset_global();

    AVAR_ASSERT_EQ(download_remove(item_id, true, true, true), EXIT_SUCCESS);
    free(item_id);
}

AVAR_TEST(download_lifecycle_invalid_operations) {
    AVAR_ASSERT_EQ(download_pause(NULL), EXIT_FAILURE);
    AVAR_ASSERT_EQ(download_resume("missing"), EXIT_FAILURE);
    AVAR_ASSERT_EQ(download_remove("missing", true, false, false), EXIT_FAILURE);
}

AVAR_TEST_MAIN(
        run_download_lifecycle_enqueue_and_remove();
        run_download_lifecycle_pause_resume_background();
        run_download_lifecycle_enqueue_proxy_and_active_list();
        run_download_lifecycle_start_stop_background();
        run_download_lifecycle_invalid_operations();)
