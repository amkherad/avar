#ifndef AVAR_TEST_GUARD_H
#define AVAR_TEST_GUARD_H

#include <stdbool.h>
#include <stddef.h>

#define AVAR_TEST_ENV_HTTP_PORT "AVAR_TEST_HTTP_PORT"
#define AVAR_TEST_ENV_RANGE_STATS_PATH "AVAR_TEST_RANGE_STATS_PATH"

typedef struct TestGuard {
    char instance_id[64];
    char work_dir[512];
    char config_path[512];
    char stats_path[512];
    int http_port;
} TestGuard;

bool test_guard_init(TestGuard *guard, const char *prefix);

void test_guard_cleanup_paths(const TestGuard *guard);

bool test_guard_bind_http_port(int *port_out);

bool test_guard_http_url(const TestGuard *guard, const char *path, char *out, size_t out_size);

void test_guard_set_server_env(const TestGuard *guard);

#endif
