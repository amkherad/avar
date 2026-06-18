#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "file-system.h"
#include "stream_hls.h"

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
static char g_playlist_url[256];
static char g_output_path[512];

#if defined(_WIN32)
static PROCESS_INFORMATION g_server_proc = {0};
#else
static pid_t g_server_pid;
#endif

static void setup_paths(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-hls-int"));
    test_guard_set_server_env(&g_guard);

    snprintf(g_server_script, sizeof g_server_script, "%s/scripts/http_test_server.py", AVAR_SOURCE_DIR);
    snprintf(g_output_path, sizeof g_output_path, "%s%cout.ts", g_guard.work_dir, PATH_SEPARATOR);
    AVAR_ASSERT(test_guard_http_url(&g_guard, "hls/playlist.m3u8", g_playlist_url, sizeof g_playlist_url));

    remove(g_output_path);
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
}

static void start_local_server(void) {
    test_guard_set_server_env(&g_guard);

#if defined(_WIN32)
    STARTUPINFOA si = {0};
    si.cb = sizeof si;
    char command[768];
    snprintf(command, sizeof command, "python \"%s\"", g_server_script);
    AVAR_ASSERT(CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si,
                               &g_server_proc));
    Sleep(3000);
#else
    g_server_pid = fork();
    AVAR_ASSERT(g_server_pid != -1);
    if (g_server_pid == 0) {
        execlp("python3", "python3", g_server_script, NULL);
        execlp("python", "python", g_server_script, NULL);
        _exit(127);
    }
    usleep(3000000);
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

AVAR_TEST(stream_hls_download_integration) {
    setup_paths();
    start_local_server();

    AVAR_ASSERT_EQ(stream_hls_download(g_playlist_url, g_output_path, NULL), 0);
    stop_local_server();

    AVAR_ASSERT(file_exists(g_output_path));
    FILE *fp = fopen(g_output_path, "rb");
    AVAR_ASSERT_NOT_NULL(fp);
    char buffer[64] = {0};
    AVAR_ASSERT(fread(buffer, 1, sizeof buffer - 1, fp) > 0U);
    fclose(fp);
    AVAR_ASSERT(strstr(buffer, "hls-segment-data") != NULL);
}

AVAR_TEST_MAIN(run_stream_hls_download_integration();)
