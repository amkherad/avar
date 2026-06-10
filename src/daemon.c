#include <daemon.h>
#include <daemon_rpc.h>
#include <daemon_transport.h>
#include <file-system.h>
#include <logger.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <io.h>
    #include <process.h>
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

typedef struct {
    DaemonTransport *http;
    DaemonTransport *https;
    DaemonTransport *pipe;
    DaemonTransport *unix_socket;
    volatile bool running;
    volatile bool reload_requested;
    DaemonConfig cfg;
} DaemonRuntime;

static DaemonRuntime _runtime = {0};

static void (*_user_ctrl_c_handler)(void) = NULL;

static void handle_ctrl_c(void);
static int install_ctrl_c_handler(void (*handler)(void));
static int remove_ctrl_c_handler(void);

#if defined(_WIN32)
static BOOL WINAPI win_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT && _user_ctrl_c_handler != NULL) {
        _user_ctrl_c_handler();
        return TRUE;
    }
    return FALSE;
}
#else
static void unix_sigint(int signo) {
    (void)signo;
    if (_user_ctrl_c_handler != NULL) {
        _user_ctrl_c_handler();
    }
}

static void unix_sighup(int signo) {
    (void)signo;
    _runtime.reload_requested = true;
}
#endif

static int install_ctrl_c_handler(void (*handler)(void)) {
    if (handler == NULL) {
        return -1;
    }
    _user_ctrl_c_handler = handler;

#if defined(_WIN32)
    if (!SetConsoleCtrlHandler(win_ctrl_handler, TRUE)) {
        return -1;
    }
#else
    struct sigaction sa = {0};
    sa.sa_handler = unix_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        return -1;
    }

    struct sigaction sa_hup = {0};
    sa_hup.sa_handler = unix_sighup;
    if (sigaction(SIGHUP, &sa_hup, NULL) == -1) {
        LOG_WARNING("Failed to install SIGHUP handler");
    }
#endif
    return 0;
}

static int remove_ctrl_c_handler(void) {
#if defined(_WIN32)
    SetConsoleCtrlHandler(win_ctrl_handler, FALSE);
#else
    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
#endif
    _user_ctrl_c_handler = NULL;
    return 0;
}

static void handle_ctrl_c(void) {
    LOG_INFO("Daemon shutdown requested");
    _runtime.running = false;
}

bool daemon_cleanup_stale_pid_file(const char *path) {
    if (path == NULL) {
        return false;
    }

    int pid = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return true;
    }

    if (fscanf(file, "%d", &pid) != 1 || pid <= 0) {
        fclose(file);
        (void)remove(path);
        return true;
    }
    fclose(file);

#if defined(_WIN32)
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (process == NULL) {
        (void)daemon_remove_pid_file(path);
        return true;
    }
    DWORD exit_code = 0;
    const BOOL alive = GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE;
    CloseHandle(process);
    if (!alive) {
        (void)daemon_remove_pid_file(path);
    }
#else
    if (kill((pid_t)pid, 0) != 0) {
        (void)daemon_remove_pid_file(path);
    }
#endif

    return true;
}

int daemon_write_pid_file(const char *path) {
    if (path == NULL) {
        return -1;
    }

#if defined(_WIN32)
    const DWORD pid = GetCurrentProcessId();
#else
    const pid_t pid = getpid();
#endif

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        LOG_ERROR("Failed to write pid file '%s'", path);
        return -1;
    }

    fprintf(file, "%d\n", (int)pid);
    fclose(file);
    return 0;
}

int daemon_remove_pid_file(const char *path) {
    if (path == NULL) {
        return -1;
    }
    if (remove(path) != 0 && errno != ENOENT) {
        LOG_WARNING("Failed to remove pid file '%s'", path);
        return -1;
    }
    return 0;
}

bool daemon_is_running(int *pid_out) {
    DaemonConfig cfg;
    daemon_config_load(&cfg);

    FILE *file = fopen(cfg.server.pid_file, "rb");
    if (file == NULL) {
        return false;
    }

    int pid = 0;
    if (fscanf(file, "%d", &pid) != 1 || pid <= 0) {
        fclose(file);
        return false;
    }
    fclose(file);

#if defined(_WIN32)
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (process == NULL) {
        return false;
    }
    DWORD exit_code = 0;
    const BOOL alive = GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE;
    CloseHandle(process);
    if (!alive) {
        return false;
    }
#else
    if (kill((pid_t)pid, 0) != 0) {
        return false;
    }
#endif

    if (pid_out != NULL) {
        *pid_out = pid;
    }
    return true;
}

static int send_stop_signal(int pid, bool force_kill) {
#if defined(_WIN32)
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (process == NULL) {
        return -1;
    }
    const BOOL ok = TerminateProcess(process, force_kill ? 1U : 0U);
    CloseHandle(process);
    return ok ? 0 : -1;
#else
    const int sig = force_kill ? SIGKILL : SIGTERM;
    return kill((pid_t)pid, sig);
#endif
}

static bool wait_for_daemon_pid(const char *pid_file, int timeout_ms) {
    if (pid_file == NULL) {
        return false;
    }

    const int step_ms = 100;
    for (int elapsed = 0; elapsed < timeout_ms; elapsed += step_ms) {
        int pid = 0;
        if (daemon_is_running(&pid)) {
            return true;
        }
#if defined(_WIN32)
        Sleep((DWORD)step_ms);
#else
        usleep((useconds_t)step_ms * 1000U);
#endif
    }

    return false;
}

#if !defined(_WIN32)
static void detach_stdio(void) {
    const int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        return;
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) {
        close(fd);
    }
}

static int container_double_fork(void) {
    const pid_t first = fork();
    if (first < 0) {
        return -1;
    }
    if (first > 0) {
        _exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        return -1;
    }

    const pid_t second = fork();
    if (second < 0) {
        return -1;
    }
    if (second > 0) {
        _exit(EXIT_SUCCESS);
    }

    detach_stdio();
    return 0;
}
#endif

int daemon_spawn_detached(const DaemonConfig *cfg) {
    if (cfg == NULL) {
        return EXIT_FAILURE;
    }

    (void)daemon_cleanup_stale_pid_file(cfg->server.pid_file);

    int existing_pid = 0;
    if (daemon_is_running(&existing_pid)) {
        LOG_ERROR("Daemon already running (pid %d)", existing_pid);
        return EXIT_FAILURE;
    }

#if defined(_WIN32)
    char exe_path[AVAR_CONFIG_PATH_MAX];
    const DWORD path_len = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
    if (path_len == 0 || path_len >= sizeof exe_path) {
        LOG_ERROR("Failed to resolve executable path");
        return EXIT_FAILURE;
    }

    char cmdline[AVAR_CONFIG_PATH_MAX * 2];
    snprintf(cmdline, sizeof cmdline,
             "\"%s\" daemon start --attached --no-detach --pipeName=\"%s\"", exe_path,
             cfg->server.pipe.name);

    STARTUPINFOA si = {0};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {0};

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                        DETACHED_PROCESS | CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        LOG_ERROR("Failed to spawn daemon process");
        return EXIT_FAILURE;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (!wait_for_daemon_pid(cfg->server.pid_file, 5000)) {
        LOG_WARNING("Daemon spawned but pid file is not ready yet");
    } else {
        int pid = 0;
        daemon_is_running(&pid);
        LOG_INFO("Daemon started in background (pid %d)", pid);
    }

    return EXIT_SUCCESS;
#else
    const pid_t child = fork();
    if (child < 0) {
        LOG_ERROR("Failed to fork daemon process");
        return EXIT_FAILURE;
    }

    if (child > 0) {
        if (!wait_for_daemon_pid(cfg->server.pid_file, 5000)) {
            LOG_WARNING("Daemon spawned (pid %d) but is not ready yet", (int)child);
        } else {
            LOG_INFO("Daemon started in background (pid %d)", (int)child);
        }
        return EXIT_SUCCESS;
    }

    if (cfg->server.container_mode) {
        if (container_double_fork() != 0) {
            _exit(EXIT_FAILURE);
        }
    } else {
        if (setsid() < 0) {
            _exit(EXIT_FAILURE);
        }
        detach_stdio();
    }

    _exit(daemon_start(cfg));
#endif
}

static void destroy_runtime_transports(void) {
    if (_runtime.http != NULL) {
        daemon_transport_destroy(_runtime.http);
        _runtime.http = NULL;
    }
    if (_runtime.https != NULL) {
        daemon_transport_destroy(_runtime.https);
        _runtime.https = NULL;
    }
    if (_runtime.pipe != NULL) {
        daemon_transport_destroy(_runtime.pipe);
        _runtime.pipe = NULL;
    }
    if (_runtime.unix_socket != NULL) {
        daemon_transport_destroy(_runtime.unix_socket);
        _runtime.unix_socket = NULL;
    }
}

static bool start_runtime_transports(const DaemonConfig *cfg) {
    if (cfg->server.http.enabled) {
        _runtime.http = daemon_transport_create(AvarTransportHttp);
        if (_runtime.http == NULL || !daemon_transport_start(_runtime.http, cfg)) {
            return false;
        }
    }

    if (cfg->server.https.enabled) {
        _runtime.https = daemon_transport_create_https();
        if (_runtime.https == NULL || !daemon_transport_start(_runtime.https, cfg)) {
            return false;
        }
    }

    if (cfg->server.pipe.enabled) {
        _runtime.pipe = daemon_transport_create(AvarTransportPipe);
        if (_runtime.pipe == NULL || !daemon_transport_start(_runtime.pipe, cfg)) {
            return false;
        }
    }

    if (cfg->server.unix_socket.enabled) {
        _runtime.unix_socket = daemon_transport_create(AvarTransportUnix);
        if (_runtime.unix_socket == NULL || !daemon_transport_start(_runtime.unix_socket, cfg)) {
            return false;
        }
    }

    return true;
}

int daemon_reload_config(DaemonConfig *cfg) {
    if (cfg == NULL) {
        return EXIT_FAILURE;
    }

    if (!daemon_config_load(cfg)) {
        LOG_ERROR("Failed to reload daemon config");
        return EXIT_FAILURE;
    }

    daemon_rpc_set_auth_token(cfg->server.auth_token[0] != '\0' ? cfg->server.auth_token : NULL);
    memcpy(&_runtime.cfg, cfg, sizeof _runtime.cfg);
    LOG_INFO("Daemon configuration reloaded");
    return EXIT_SUCCESS;
}

int daemon_start(const DaemonConfig *cfg) {
    if (cfg == NULL) {
        return EXIT_FAILURE;
    }

    (void)daemon_cleanup_stale_pid_file(cfg->server.pid_file);

    int existing_pid = 0;
    if (daemon_is_running(&existing_pid)) {
        LOG_ERROR("Daemon already running (pid %d)", existing_pid);
        return EXIT_FAILURE;
    }

    memcpy(&_runtime.cfg, cfg, sizeof _runtime.cfg);
    _runtime.running = true;
    _runtime.reload_requested = false;

    daemon_rpc_init();
    daemon_rpc_set_auth_token(cfg->server.auth_token[0] != '\0' ? cfg->server.auth_token : NULL);

    if (!start_runtime_transports(cfg)) {
        destroy_runtime_transports();
        return EXIT_FAILURE;
    }

    if (daemon_write_pid_file(cfg->server.pid_file) != 0) {
        destroy_runtime_transports();
        return EXIT_FAILURE;
    }

    install_ctrl_c_handler(handle_ctrl_c);
    LOG_INFO("Daemon started (pid file: %s)", cfg->server.pid_file);

    while (_runtime.running) {
        if (_runtime.reload_requested) {
            _runtime.reload_requested = false;
            (void)daemon_reload_config(&_runtime.cfg);
        }

        if (_runtime.http != NULL) {
            daemon_transport_poll(_runtime.http, AVAR_DAEMON_POLL_MS);
        } else if (_runtime.https != NULL) {
            daemon_transport_poll(_runtime.https, AVAR_DAEMON_POLL_MS);
        } else {
#if defined(_WIN32)
            Sleep((DWORD)AVAR_DAEMON_POLL_MS);
#else
            usleep((useconds_t)AVAR_DAEMON_POLL_MS * 1000U);
#endif
        }
    }

    LOG_INFO("Draining active downloads before shutdown");
    daemon_rpc_shutdown();

    remove_ctrl_c_handler();
    destroy_runtime_transports();
    daemon_remove_pid_file(cfg->server.pid_file);
    LOG_INFO("Daemon stopped");
    return EXIT_SUCCESS;
}

int daemon_stop(const DaemonStopOptions *opts) {
    const bool wait = opts != NULL && opts->wait;
    const bool force_kill = opts != NULL && opts->force_kill;

    int pid = 0;
    if (!daemon_is_running(&pid)) {
        LOG_INFO("Daemon is not running");
        return EXIT_SUCCESS;
    }

    if (send_stop_signal(pid, force_kill) != 0) {
        LOG_ERROR("Failed to stop daemon (pid %d)", pid);
        return EXIT_FAILURE;
    }

    if (!wait) {
        LOG_INFO("Stop signal sent to daemon (pid %d)", pid);
        return EXIT_SUCCESS;
    }

    for (int i = 0; i < 100; ++i) {
        if (!daemon_is_running(NULL)) {
            LOG_INFO("Daemon stopped");
            return EXIT_SUCCESS;
        }
#if defined(_WIN32)
        Sleep(100);
#else
        usleep(100000);
#endif
    }

    LOG_ERROR("Timed out waiting for daemon to stop");
    return EXIT_FAILURE;
}

int daemon_restart(const DaemonConfig *cfg) {
    DaemonStopOptions stop_opts = {.wait = true, .force_kill = false};
    const int stop_rc = daemon_stop(&stop_opts);
    if (stop_rc != EXIT_SUCCESS) {
        return stop_rc;
    }
    return daemon_start(cfg);
}

int start_daemon(void) {
    DaemonConfig cfg;
    daemon_config_load(&cfg);
    return daemon_start(&cfg);
}

int stop_daemon(void) {
    return daemon_stop(NULL);
}

int restart_daemon(void) {
    DaemonConfig cfg;
    daemon_config_load(&cfg);
    return daemon_restart(&cfg);
}
