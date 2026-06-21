#include <cli/gui_launch.h>
#include <cli/electron_embed.h>

#include <avar.h>
#include <cli.h>
#include <daemon/daemon.h>
#include <logger.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <shellapi.h>
    #include <process.h>
#else
    #include <pthread.h>
    #include <spawn.h>
    #include <sys/socket.h>
    #include <sys/wait.h>
    #include <unistd.h>

    #if defined(__APPLE__)
        #include <mach-o/dyld.h>
    #endif

    extern char **environ;
#endif

typedef struct {
    char host[64];
    uint16_t port;
} GuiLaunchContext;

typedef enum {
    GuiLaunchFailed = -1,
    GuiLaunchBrowser = 0,
    GuiLaunchElectron = 1,
} GuiLaunchMode;

#if defined(_WIN32)
unsigned __stdcall gui_ui_launch_worker(void *arg);
#else
void *gui_ui_launch_worker(void *arg);
#endif

static bool is_gui_invocation(int argc, char *argv[]);
static void print_gui_help(void);
static int run_embedded_gui(int argc, char *argv[]);
static bool wait_for_tcp_port(const char *host, uint16_t port, unsigned attempts);
static GuiLaunchMode launch_gui_frontend(const char *url);
static int resolve_exe_dir(char *buf, size_t buflen);
static bool resolve_electron_path(const char *exe_dir, char *out, size_t outlen);

int cli_embed_gui_early(int argc, char *argv[]) {
    if (!is_gui_invocation(argc, argv)) {
        return -1;
    }

    if (argc >= 2 && (cli_is_help_flag(argv[1]) ||
                      (argc >= 3 && strcmp(argv[1], "gui") == 0 && cli_is_help_flag(argv[2])))) {
        print_gui_help();
        return EXIT_SUCCESS;
    }

    return run_embedded_gui(argc, argv);
}

static bool is_gui_invocation(int argc, char *argv[]) {
    if (argc < 2) {
        return true;
    }

    if (strcmp(argv[1], "gui") == 0) {
        return true;
    }

    return false;
}

static void print_gui_help(void) {
    puts("Avar Download Manager - GUI");
    puts("");
    puts("Usage:");
    puts("  avar");
    puts("  avar gui");
    puts("");
    puts("Starts the download daemon in-process with the embedded web UI and opens");
    puts("the embedded Electron shell when available, otherwise the system browser.");
    puts("");
    puts("Environment:");
    puts("  AVAR_ELECTRON       Override path to the Electron executable");
    puts("  AVAR_ELECTRON_CACHE Override extraction cache directory");
    puts("  AVAR_GUI_URL        Override GUI URL passed to Electron");
}

static int run_embedded_gui(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    DaemonConfig cfg;
    daemon_config_load(&cfg);
    cfg.server.detach = false;
    cfg.server.http.enabled = true;

    const DaemonStartOptions opts = {
        .foreground = true,
        .http_override = true,
        .no_detach = true,
    };
    daemon_config_apply_start_options(&cfg, &opts);

    GuiLaunchContext *launch_ctx = calloc(1, sizeof *launch_ctx);
    if (launch_ctx == NULL) {
        return EXIT_FAILURE;
    }

    snprintf(launch_ctx->host, sizeof launch_ctx->host, "%s", cfg.server.http.bind_addr);
    launch_ctx->port = cfg.server.http.port;

#if defined(_WIN32)
    const uintptr_t thread =
            _beginthreadex(NULL, 0, gui_ui_launch_worker, launch_ctx, 0, NULL);
    if (thread == 0U) {
        free(launch_ctx);
        return EXIT_FAILURE;
    }
    CloseHandle((HANDLE)thread);
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, gui_ui_launch_worker, launch_ctx) != 0) {
        free(launch_ctx);
        return EXIT_FAILURE;
    }
    pthread_detach(thread);
#endif

    return daemon_start(&cfg);
}

#if defined(_WIN32)
unsigned __stdcall gui_ui_launch_worker(void *arg) {
#else
void *gui_ui_launch_worker(void *arg) {
#endif
    GuiLaunchContext *ctx = (GuiLaunchContext *)arg;
    if (ctx == NULL) {
#if defined(_WIN32)
        return 0;
#else
        return NULL;
#endif
    }

    char url[256];
    snprintf(url, sizeof url, "http://%s:%u/", ctx->host, (unsigned)ctx->port);

    if (!wait_for_tcp_port(ctx->host, ctx->port, 100U)) {
        LOG_WARNING("Timed out waiting for daemon HTTP port %u", (unsigned)ctx->port);
    }

    const GuiLaunchMode mode = launch_gui_frontend(url);
    free(ctx);

    if (mode == GuiLaunchElectron || mode == GuiLaunchFailed) {
        daemon_request_shutdown();
    }

#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

static bool wait_for_tcp_port(const char *host, const uint16_t port, const unsigned attempts) {
    for (unsigned i = 0; i < attempts; ++i) {
        struct addrinfo hints = {0};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo *res = NULL;
        char port_buf[16];
        snprintf(port_buf, sizeof port_buf, "%u", (unsigned)port);

        if (getaddrinfo(host, port_buf, &hints, &res) != 0 || res == NULL) {
#if defined(_WIN32)
            Sleep(100);
#else
            usleep(100000);
#endif
            continue;
        }

        const int sock = (int)socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock >= 0) {
            const bool connected = connect(sock, res->ai_addr, (int)res->ai_addrlen) == 0;
#if defined(_WIN32)
            closesocket(sock);
#else
            close(sock);
#endif
            freeaddrinfo(res);
            if (connected) {
                return true;
            }
        } else {
            freeaddrinfo(res);
        }

#if defined(_WIN32)
        Sleep(100);
#else
        usleep(100000);
#endif
    }

    return false;
}

static int resolve_exe_dir(char *buf, const size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return -1;
    }

#if defined(_WIN32)
    const DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)buflen);
    if (n == 0 || n >= buflen) {
        return -1;
    }
    char *slash = strrchr(buf, '\\');
    if (slash == NULL) {
        return -1;
    }
    *slash = '\0';
    return 0;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)buflen;
    if (_NSGetExecutablePath(buf, &size) != 0) {
        return -1;
    }
    char *slash = strrchr(buf, '/');
    if (slash == NULL) {
        return -1;
    }
    *slash = '\0';
    return 0;
#else
    const ssize_t n = readlink("/proc/self/exe", buf, buflen - 1U);
    if (n <= 0) {
        return -1;
    }
    buf[(size_t)n] = '\0';
    char *slash = strrchr(buf, '/');
    if (slash == NULL) {
        return -1;
    }
    *slash = '\0';
    return 0;
#endif
}

static bool resolve_electron_path(const char *exe_dir, char *out, const size_t outlen) {
    const char *env_path = getenv("AVAR_ELECTRON");
    if (env_path != NULL && env_path[0] != '\0') {
        snprintf(out, outlen, "%s", env_path);
        return true;
    }

    static const char *candidates[] = {
#if defined(_WIN32)
        "electron\\Avar.exe",
        "electron\\avar.exe",
#elif defined(__APPLE__)
        "electron/Avar.app/Contents/MacOS/Avar",
#else
        "electron/avar",
        "electron/Avar",
#endif
    };

    for (size_t i = 0; i < sizeof candidates / sizeof candidates[0]; ++i) {
        snprintf(out, outlen, "%s/%s", exe_dir, candidates[i]);
#if defined(_WIN32)
        if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
#else
        if (access(out, X_OK) == 0) {
            return true;
        }
#endif
    }

    return false;
}

static GuiLaunchMode launch_gui_frontend(const char *url) {
    if (url == NULL || url[0] == '\0') {
        return GuiLaunchFailed;
    }

    const char *override_url = getenv("AVAR_GUI_URL");
    const char *gui_url = (override_url != NULL && override_url[0] != '\0') ? override_url : url;

    char electron_path[AVAR_CONFIG_PATH_MAX];
    char work_dir[AVAR_CONFIG_PATH_MAX];
    work_dir[0] = '\0';

    const char *env_path = getenv("AVAR_ELECTRON");
    if (env_path != NULL && env_path[0] != '\0') {
        snprintf(electron_path, sizeof electron_path, "%s", env_path);
    } else if (electron_embed_resolve_exe(electron_path, sizeof electron_path)) {
        const char *slash = strrchr(electron_path,
#if defined(_WIN32)
                                    '\\');
#else
                                    '/');
#endif
        if (slash != NULL) {
            const size_t dir_len = (size_t)(slash - electron_path);
            if (dir_len < sizeof work_dir) {
                memcpy(work_dir, electron_path, dir_len);
                work_dir[dir_len] = '\0';
            }
        }
    } else {
        char exe_dir[AVAR_CONFIG_PATH_MAX];
        if (resolve_exe_dir(exe_dir, sizeof exe_dir) != 0 ||
            !resolve_electron_path(exe_dir, electron_path, sizeof electron_path)) {
            electron_path[0] = '\0';
        } else {
            snprintf(work_dir, sizeof work_dir, "%s", exe_dir);
        }
    }

    if (electron_path[0] != '\0') {
        const char *cwd = work_dir[0] != '\0' ? work_dir : NULL;
#if defined(_WIN32)
        _putenv_s("AVAR_GUI_URL", gui_url);
        _putenv_s("AVAR_BUNDLED", "1");

        char cmdline[AVAR_CONFIG_PATH_MAX * 2];
        snprintf(cmdline, sizeof cmdline, "\"%s\"", electron_path);

        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof si;

        if (CreateProcessA(electron_path, cmdline, NULL, NULL, TRUE, 0, NULL, cwd, &si, &pi)) {
            CloseHandle(pi.hThread);
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            LOG_INFO("Electron shell exited");
            return GuiLaunchElectron;
        }

        LOG_WARNING("Failed to launch Electron at %s", electron_path);
#else
        setenv("AVAR_GUI_URL", gui_url, 1);
        setenv("AVAR_BUNDLED", "1", 1);

        char *const argv[] = {(char *)electron_path, NULL};
        pid_t pid = 0;
        const int spawn_rc = posix_spawn(&pid, electron_path, NULL, NULL, argv, environ);
        if (spawn_rc == 0 && pid > 0) {
            int status = 0;
            (void)waitpid(pid, &status, 0);
            LOG_INFO("Electron shell exited");
            return GuiLaunchElectron;
        }

        LOG_WARNING("Failed to launch Electron at %s", electron_path);
#endif
    }

#if defined(_WIN32)
    const HINSTANCE rc = ShellExecuteA(NULL, "open", gui_url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)rc > 32) {
        LOG_INFO("Opened GUI in system browser");
        return GuiLaunchBrowser;
    }
#elif defined(__APPLE__)
    char cmd[512];
    snprintf(cmd, sizeof cmd, "open '%s'", gui_url);
    if (system(cmd) == 0) {
        LOG_INFO("Opened GUI in system browser");
        return GuiLaunchBrowser;
    }
#else
    char cmd[512];
    snprintf(cmd, sizeof cmd, "xdg-open '%s'", gui_url);
    if (system(cmd) == 0) {
        LOG_INFO("Opened GUI in system browser");
        return GuiLaunchBrowser;
    }
#endif

    LOG_ERROR("Could not open GUI (no embedded Electron and browser launch failed)");
    return GuiLaunchFailed;
}
