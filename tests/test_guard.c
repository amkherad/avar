#include "test_guard.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "mongoose_log.h"

#if defined(_WIN32)
    #include <direct.h>
    #include <process.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/stat.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

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

bool test_guard_bind_http_port(int *port_out) {
    if (port_out == NULL) {
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

    *port_out = (int)ntohs(addr.sin_port);
#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif
    return *port_out > 0;
}

bool test_guard_init(TestGuard *guard, const char *prefix) {
    if (guard == NULL || prefix == NULL || prefix[0] == '\0') {
        return false;
    }

    memset(guard, 0, sizeof *guard);
    snprintf(guard->instance_id, sizeof guard->instance_id, "%s-%d", prefix, test_guard_process_id());

    const char *base = test_guard_temp_base();
    snprintf(guard->work_dir, sizeof guard->work_dir, "%s%c%s", base, PATH_SEPARATOR,
             guard->instance_id);
    snprintf(guard->config_path, sizeof guard->config_path, "%s%cconfig.json", guard->work_dir,
             PATH_SEPARATOR);
    snprintf(guard->stats_path, sizeof guard->stats_path, "%s%crange-stats.json", guard->work_dir,
             PATH_SEPARATOR);

    if (!test_guard_bind_http_port(&guard->http_port)) {
        return false;
    }

    if (!test_guard_make_dir(guard->work_dir)) {
        return false;
    }

    avar_mongoose_log_silence();

    return true;
}

void test_guard_cleanup_paths(const TestGuard *guard) {
    if (guard == NULL) {
        return;
    }

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
