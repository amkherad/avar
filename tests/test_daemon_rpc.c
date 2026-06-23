#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include <daemon/daemon_rpc.h>
#include "file-system.h"
#include "logger.h"

static TestGuard g_guard;

static void setup_rpc_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-rpc"));
    remove(g_guard.config_path);
    init_logger(false);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);

    char download_path[512];
    snprintf(download_path, sizeof download_path, "%s%cdownloads", g_guard.work_dir, PATH_SEPARATOR);
    (void)make_dirs_in_path(download_path);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_DOWNLOAD_PATH, download_path), 0);
    daemon_rpc_init();
}

static bool rpc_call(const char *method, const char *params_json, char **response_out) {
    char request[1024];
    const int written = snprintf(request, sizeof request,
                                 "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":1}",
                                 method, params_json != NULL ? params_json : "{}");
    if (written <= 0 || (size_t)written >= sizeof request) {
        return false;
    }
    return daemon_rpc_handle(request, response_out);
}

AVAR_TEST(daemon_rpc_ping_and_health) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(rpc_call("ping", NULL, &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "ok") != NULL);
    free(response);

    AVAR_ASSERT(rpc_call("health", NULL, &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);
}

AVAR_TEST(daemon_rpc_system_stats_and_lists) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(rpc_call("system.stats", NULL, &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);

    AVAR_ASSERT(rpc_call("queue.list", NULL, &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);

    AVAR_ASSERT(rpc_call("downloads.list", NULL, &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);
}

AVAR_TEST(daemon_rpc_queue_mutations) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(rpc_call("queue.add",
                          "{\"name\":\"rpc-queue\",\"description\":\"rpc test\"}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);

    AVAR_ASSERT(rpc_call("queue.list", NULL, &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "rpc-queue") != NULL);
    free(response);

    AVAR_ASSERT(rpc_call("queue.edit",
                          "{\"id\":\"1\",\"description\":\"updated\"}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);

    AVAR_ASSERT(rpc_call("queue.stop", "{\"id\":\"1\"}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);

    AVAR_ASSERT(rpc_call("queue.start", "{\"id\":\"1\"}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);

    AVAR_ASSERT(rpc_call("queue.remove", "{\"id\":\"1\"}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);
}

AVAR_TEST(daemon_rpc_cli_exec_and_logs) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(rpc_call("cli.exec", "{\"argv\":[\"config\",\"get\",\"dm.downloadPath\"]}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);

    AVAR_ASSERT(rpc_call("logs.get", "{\"offset\":0,\"limit\":10}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);
}

AVAR_TEST(daemon_rpc_fs_browse) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(rpc_call("fs.browse", "{}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "Method not found") != NULL);
    free(response);

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SERVER_FS_BROWSE_ENABLED, "true"), 0);

    AVAR_ASSERT(rpc_call("fs.browse", "{\"path\":\"\"}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "entries") != NULL);
    free(response);
}

AVAR_TEST(daemon_rpc_invalid_request) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(!daemon_rpc_handle("not-json", &response));
    AVAR_ASSERT_NULL(response);

    AVAR_ASSERT(rpc_call("missing.method", NULL, &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "Method not found") != NULL);
    free(response);
}

AVAR_TEST(daemon_rpc_reload_config) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(rpc_call("daemon.reload", NULL, &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);
}

AVAR_TEST(daemon_rpc_http_endpoints) {
    setup_rpc_config();

    char *body = NULL;
    int status = 0;

    AVAR_ASSERT(daemon_rpc_handle_http("/api/health", NULL, 0, &body, &status));
    AVAR_ASSERT_EQ(status, 200);
    AVAR_ASSERT_NOT_NULL(body);
    free(body);

    AVAR_ASSERT(daemon_rpc_handle_http("/api/stats", NULL, 0, &body, &status));
    AVAR_ASSERT_EQ(status, 200);
    AVAR_ASSERT_NOT_NULL(body);
    free(body);

    const char *rpc_body = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"params\":{},\"id\":1}";
    AVAR_ASSERT(daemon_rpc_handle_http("/api/rpc", rpc_body, strlen(rpc_body), &body, &status));
    AVAR_ASSERT_EQ(status, 200);
    AVAR_ASSERT_NOT_NULL(body);
    free(body);

    AVAR_ASSERT(daemon_rpc_handle_http("/api/missing", NULL, 0, &body, &status));
    AVAR_ASSERT_EQ(status, 404);
    AVAR_ASSERT_NOT_NULL(body);
    free(body);
}

AVAR_TEST(daemon_rpc_download_add) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(rpc_call("download.add",
                          "{\"url\":\"http://127.0.0.1:1/missing\",\"queue\":\"default\"}",
                          &response));
    AVAR_ASSERT_NOT_NULL(response);
    free(response);
}

AVAR_TEST(daemon_rpc_download_progress_watch) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(rpc_call("download.watch", "{\"id\":\"dl-watch-test\"}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "\"exitCode\":0") != NULL);
    free(response);

    AVAR_ASSERT(rpc_call("download.unwatch", "{\"id\":\"dl-watch-test\"}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "\"exitCode\":0") != NULL);
    free(response);
}

AVAR_TEST(daemon_rpc_malformed_jsonrpc) {
    setup_rpc_config();

    char *response = NULL;
    AVAR_ASSERT(daemon_rpc_handle("{\"jsonrpc\":\"1.0\",\"method\":\"ping\",\"id\":1}", &response));
    AVAR_ASSERT_NOT_NULL(response);
    AVAR_ASSERT(strstr(response, "Invalid Request") != NULL);
    free(response);
}

AVAR_TEST_MAIN(
        run_daemon_rpc_ping_and_health();
        run_daemon_rpc_system_stats_and_lists();
        run_daemon_rpc_queue_mutations();
        run_daemon_rpc_cli_exec_and_logs();
        run_daemon_rpc_fs_browse();
        run_daemon_rpc_invalid_request();
        run_daemon_rpc_reload_config();
        run_daemon_rpc_http_endpoints();
        run_daemon_rpc_download_add();
        run_daemon_rpc_download_progress_watch();
        run_daemon_rpc_malformed_jsonrpc();)
