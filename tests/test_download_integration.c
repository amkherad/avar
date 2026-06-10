#include "avar_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "download.h"
#include "file-system.h"

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

static char g_config_path[512];
static char g_temp_dir[512];
static char g_download_dir[512];
static char g_server_script[512];

#if !defined(_WIN32)
static pid_t g_server_pid;
#endif

static void setup_isolated_paths(void) {
#if defined(_WIN32)
    const char *base = getenv("TEMP");
#else
    const char *base = "/tmp";
#endif
    if (base == NULL) {
        base = ".";
    }

    snprintf(g_config_path, sizeof g_config_path, "%s%cavar-dl-test-config.json", base,
             PATH_SEPARATOR);
    snprintf(g_temp_dir, sizeof g_temp_dir, "%s%cavar-dl-test-temp", base, PATH_SEPARATOR);
    snprintf(g_download_dir, sizeof g_download_dir, "%s%cavar-dl-test-download", base,
             PATH_SEPARATOR);
    snprintf(g_server_script, sizeof g_server_script, "%s/scripts/http_test_server.py",
             AVAR_SOURCE_DIR);

    remove(g_config_path);
    (void) make_dirs_in_path(g_temp_dir);
    (void) make_dirs_in_path(g_download_dir);

    char *dest = path_join(g_download_dir, "plain.txt");
    char *temp = path_join(g_temp_dir, "plain.txt");
    if (dest != NULL) {
        remove(dest);
        free(dest);
    }
    if (temp != NULL) {
        remove(temp);
        free(temp);
    }

    AVAR_ASSERT_EQ(config_open_at(g_config_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_TEMP_PATH, g_temp_dir), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_DOWNLOAD_PATH, g_download_dir), 0);
}

static void start_local_server(void) {
#if defined(_WIN32)
    char command[768];
    snprintf(command, sizeof command, "start /B python \"%s\"", g_server_script);
    AVAR_ASSERT_EQ(system(command), 0);
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
#if !defined(_WIN32)
    if (g_server_pid > 0) {
        kill(g_server_pid, SIGTERM);
        waitpid(g_server_pid, NULL, 0);
    }
#endif
}

AVAR_TEST(download_integration_redirect_follow) {
    setup_isolated_paths();
    start_local_server();

    const int rc = transient_download("http://127.0.0.1:18080/redirect.bin", NULL, NULL, true);
    stop_local_server();

    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);

    char *dest = path_join(g_download_dir, "plain.txt");
    AVAR_ASSERT_NOT_NULL(dest);
    AVAR_ASSERT(file_exists(dest));
    free(dest);
}

AVAR_TEST(download_integration_attached_roundtrip) {
    setup_isolated_paths();
    start_local_server();

    const int rc = transient_download("http://127.0.0.1:18080/plain.txt", NULL, NULL, true);
    stop_local_server();

    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);

    char *dest = path_join(g_download_dir, "plain.txt");
    AVAR_ASSERT_NOT_NULL(dest);
    AVAR_ASSERT(file_exists(dest));

    FILE *file = fopen(dest, "rb");
    AVAR_ASSERT_NOT_NULL(file);
    char buffer[64] = {0};
    AVAR_ASSERT_EQ(fread(buffer, 1, sizeof buffer - 1, file), 11U);
    fclose(file);
    AVAR_ASSERT_STR_EQ(buffer, "hello world");

    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 1);
    char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_STATUS);
    AVAR_ASSERT_NOT_NULL(status);
    AVAR_ASSERT_STR_EQ(status, AVAR_DL_STATUS_COMPLETED);
    free(status);

    free(dest);
}

AVAR_TEST_MAIN(
        run_download_integration_attached_roundtrip();
        run_download_integration_redirect_follow();)
