#include "test_guard.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "logger.h"
#include "mongoose_log.h"

#if defined(_WIN32)
    #include <direct.h>
    #include <process.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <signal.h>
    #include <sys/stat.h>
#if defined(__linux__)
    #include <sys/prctl.h>
#endif
    #include <sys/socket.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#define TEST_GUARD_SERVER_READY_MS 30000
#define TEST_GUARD_SERVER_POLL_MS 100

static bool test_guard_tcp_connect(int port) {
    if (port <= 0) {
        return false;
    }

#if defined(_WIN32)
    static bool winsock_ready = false;
    if (!winsock_ready) {
        WSADATA wsa = {0};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            return false;
        }
        winsock_ready = true;
    }
#endif

    const int sock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return false;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    const bool connected = connect(sock, (struct sockaddr *)&addr, sizeof addr) == 0;
#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif
    return connected;
}

#if defined(_WIN32)
static bool test_guard_process_running(void *process) {
    if (process == NULL) {
        return false;
    }

    DWORD exit_code = 0;
    if (!GetExitCodeProcess((HANDLE)process, &exit_code)) {
        return false;
    }

    return exit_code == STILL_ACTIVE;
}
#else
static bool test_guard_process_running(int pid) {
    if (pid <= 0) {
        return false;
    }

    int status = 0;
    const pid_t result = waitpid(pid, &status, WNOHANG);
    return result == 0;
}
#endif

static bool test_guard_http_request(const TestGuard *guard, const char *request,
                                    int *status_out) {
    if (guard == NULL || request == NULL || guard->http_port <= 0) {
        return false;
    }

    const int sock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return false;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)guard->http_port);

    if (connect(sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    if (send(sock, request, (int)strlen(request), 0) < 0) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    char response[64] = {0};
    const int received = (int)recv(sock, response, (int)sizeof response - 1, 0);
#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif

    if (received <= 0) {
        return false;
    }

    response[received] = '\0';
    int status = 0;
    if (sscanf(response, "HTTP/%*s %d", &status) != 1) {
        return false;
    }

    if (status_out != NULL) {
        *status_out = status;
    }

    return true;
}

static bool test_guard_http_get_status(const TestGuard *guard, const char *path, int expected_status) {
    if (guard == NULL || path == NULL || guard->http_port <= 0) {
        return false;
    }

    char request[256];
    snprintf(request, sizeof request,
             "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", path);

    int status = 0;
    return test_guard_http_request(guard, request, &status) && status == expected_status;
}

static bool test_guard_http_server_ready(const TestGuard *guard) {
    return test_guard_http_get_status(guard, "/plain.txt", 200);
}

static bool test_guard_wait_http_server(const TestGuard *guard, TestHttpServer *server) {
    if (guard == NULL || server == NULL || guard->http_port <= 0) {
        return false;
    }

    for (int elapsed = 0; elapsed < TEST_GUARD_SERVER_READY_MS; elapsed += TEST_GUARD_SERVER_POLL_MS) {
#if defined(_WIN32)
        if (!test_guard_process_running(server->process)) {
            return false;
        }
#else
        if (!test_guard_process_running(server->pid)) {
            return false;
        }
#endif
        if (test_guard_tcp_connect(guard->http_port) && test_guard_http_server_ready(guard)) {
            return true;
        }
#if defined(_WIN32)
        Sleep((DWORD)TEST_GUARD_SERVER_POLL_MS);
#else
        usleep((useconds_t)TEST_GUARD_SERVER_POLL_MS * 1000U);
#endif
    }

    return test_guard_tcp_connect(guard->http_port) && test_guard_http_server_ready(guard);
}

static bool test_guard_make_dir(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }

#if defined(_WIN32)
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

static const char *test_guard_temp_base(void) {
#if defined(_WIN32)
    const char *base = getenv(AVAR_ENV_TEMP);
#else
    const char *base = "/tmp";
#endif
    return base != NULL ? base : ".";
}

static int test_guard_process_id(void) {
#if defined(_WIN32)
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

static bool test_guard_bind_http_port(TestGuard *guard) {
    if (guard == NULL) {
        return false;
    }

    test_guard_release_http_port(guard);

#if defined(_WIN32)
    static bool winsock_ready = false;
    if (!winsock_ready) {
        WSADATA wsa = {0};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            return false;
        }
        winsock_ready = true;
    }
#endif

    const int sock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return false;
    }

    int yes = 1;
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    socklen_t addr_len = sizeof addr;
    if (getsockname(sock, (struct sockaddr *)&addr, &addr_len) != 0) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    guard->http_port = (int)ntohs(addr.sin_port);
    guard->http_port_sock = sock;
    return guard->http_port > 0;
}

void test_guard_release_http_port(TestGuard *guard) {
    if (guard == NULL || guard->http_port_sock < 0) {
        return;
    }

#if defined(_WIN32)
    closesocket((SOCKET)guard->http_port_sock);
#else
    close(guard->http_port_sock);
#endif
    guard->http_port_sock = -1;
}

bool test_guard_init(TestGuard *guard, const char *prefix) {
    if (guard == NULL || prefix == NULL || prefix[0] == '\0') {
        return false;
    }

    memset(guard, 0, sizeof *guard);
    guard->http_port_sock = -1;
    snprintf(guard->instance_id, sizeof guard->instance_id, "%s-%d", prefix, test_guard_process_id());

    const char *base = test_guard_temp_base();
    snprintf(guard->work_dir, sizeof guard->work_dir, "%s%c%s", base, PATH_SEPARATOR,
             guard->instance_id);
    snprintf(guard->config_path, sizeof guard->config_path, "%s%cconfig.json", guard->work_dir,
             PATH_SEPARATOR);
    snprintf(guard->stats_path, sizeof guard->stats_path, "%s%crange-stats.json", guard->work_dir,
             PATH_SEPARATOR);

    if (!test_guard_bind_http_port(guard)) {
        return false;
    }

    if (!test_guard_make_dir(guard->work_dir)) {
        return false;
    }

    init_logger(false);
    avar_mongoose_log_silence();

    return true;
}

void test_guard_cleanup_paths(TestGuard *guard) {
    if (guard == NULL) {
        return;
    }

    test_guard_release_http_port(guard);
    remove(guard->config_path);
    remove(guard->stats_path);
}

bool test_guard_http_url(const TestGuard *guard, const char *path, char *out, size_t out_size) {
    if (guard == NULL || path == NULL || out == NULL || out_size == 0 || guard->http_port <= 0) {
        return false;
    }

    const char *resource = path[0] == '/' ? path + 1 : path;
    const int written =
        snprintf(out, out_size, "http://127.0.0.1:%d/%s", guard->http_port, resource);
    return written > 0 && (size_t)written < out_size;
}

void test_guard_set_server_env(const TestGuard *guard) {
    if (guard == NULL) {
        return;
    }

    char port_value[16];
    snprintf(port_value, sizeof port_value, "%d", guard->http_port);

#if defined(_WIN32)
    char port_entry[64];
    char stats_entry[640];

    snprintf(port_entry, sizeof port_entry, "%s=%s", AVAR_TEST_ENV_HTTP_PORT, port_value);
    snprintf(stats_entry, sizeof stats_entry, "%s=%s", AVAR_TEST_ENV_RANGE_STATS_PATH,
             guard->stats_path);
    (void)_putenv(port_entry);
    (void)_putenv(stats_entry);
#else
    (void)setenv(AVAR_TEST_ENV_HTTP_PORT, port_value, 1);
    (void)setenv(AVAR_TEST_ENV_RANGE_STATS_PATH, guard->stats_path, 1);
#endif
}

void test_guard_http_server_init(TestHttpServer *server) {
    if (server == NULL) {
        return;
    }

    memset(server, 0, sizeof *server);
}

bool test_guard_http_server_start(const TestGuard *guard, const char *script_path,
                                  TestHttpServer *server) {
    if (guard == NULL || script_path == NULL || server == NULL || script_path[0] == '\0') {
        return false;
    }

    if (server->running) {
        test_guard_http_server_stop(server);
    }

    test_guard_release_http_port((TestGuard *)guard);
    test_guard_set_server_env(guard);

#if defined(_WIN32)
    STARTUPINFOA si = {0};
    si.cb = sizeof si;
    char command[768];
    snprintf(command, sizeof command, "python \"%s\"", script_path);

    PROCESS_INFORMATION proc = {0};
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si,
                        &proc)) {
        return false;
    }

    server->process = proc.hProcess;
    server->thread = proc.hThread;
#else
    const pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
#if defined(__linux__)
        (void)prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
        const char *python = getenv("PYTHON");
        if (python == NULL || python[0] == '\0') {
            python = "python3";
        }
        execlp(python, python, script_path, (char *)NULL);
        execlp("python3", "python3", script_path, (char *)NULL);
        execlp("python", "python", script_path, (char *)NULL);
        _exit(127);
    }

    server->pid = (int)pid;
#endif

    if (!test_guard_wait_http_server(guard, server)) {
        test_guard_http_server_stop(server);
        return false;
    }

    server->running = true;
    return true;
}

void test_guard_http_server_stop(TestHttpServer *server) {
    if (server == NULL || !server->running) {
        return;
    }

#if defined(_WIN32)
    if (server->process != NULL) {
        TerminateProcess((HANDLE)server->process, 0);
        WaitForSingleObject((HANDLE)server->process, 2000);
        CloseHandle((HANDLE)server->process);
        CloseHandle((HANDLE)server->thread);
    }
#else
    if (server->pid > 0) {
        kill(server->pid, SIGTERM);
        waitpid(server->pid, NULL, 0);
    }
#endif

    test_guard_http_server_init(server);
}

bool test_guard_http_server_reset_stats(const TestGuard *guard) {
    return test_guard_http_get_status(guard, "/test_reset", 204);
}

bool test_guard_http_server_is_ready(const TestGuard *guard) {
    return test_guard_http_server_ready(guard);
}
