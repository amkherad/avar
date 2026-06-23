#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "download.h"
#include "file-system.h"
#include "queue.h"
#include "thread_pool.h"

#ifndef AVAR_SOURCE_DIR
#define AVAR_SOURCE_DIR "."
#endif

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

static TestGuard g_guard;
static TestHttpServer g_http_server;
static char g_temp_dir[512];
static char g_download_dir[512];
static char g_server_script[512];
static bool g_fixture_ready = false;

static bool setup_paths(void) {
    if (!g_fixture_ready) {
        if (!test_guard_init(&g_guard, "avar-dl-lifecycle")) {
            return false;
        }
        snprintf(g_server_script, sizeof g_server_script, "%s/scripts/http_test_server.py",
                 AVAR_SOURCE_DIR);
        test_guard_http_server_init(&g_http_server);
        if (!test_guard_http_server_start(&g_guard, g_server_script, &g_http_server)) {
            return false;
        }
        g_fixture_ready = true;
    }

    if (!test_guard_http_server_is_ready(&g_guard)) {
        return false;
    }

    snprintf(g_temp_dir, sizeof g_temp_dir, "%s%ctemp", g_guard.work_dir, PATH_SEPARATOR);
    snprintf(g_download_dir, sizeof g_download_dir, "%s%cdownload", g_guard.work_dir,
             PATH_SEPARATOR);

    (void)make_dirs_in_path(g_temp_dir);
    (void)make_dirs_in_path(g_download_dir);

    remove(g_guard.config_path);
    if (config_open_at(g_guard.config_path) != 0) {
        return false;
    }
    if (set_config(AVAR_CFG_DM_TEMP_PATH, g_temp_dir) != 0
        || set_config(AVAR_CFG_DM_DOWNLOAD_PATH, g_download_dir) != 0) {
        return false;
    }

    return true;
}

static void build_url(const char *path, char *out, size_t out_size) {
    AVAR_ASSERT(test_guard_http_url(&g_guard, path, out, out_size));
}

static char *find_item_status(const char *item_id) {
    if (item_id == NULL) {
        return NULL;
    }

    const size_t count = get_config_array_size(AVAR_CFG_DM_ITEMS);
    for (size_t i = 0; i < count; ++i) {
        char *id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_ID);
        if (id == NULL) {
            continue;
        }

        const bool match = strcmp(id, item_id) == 0;
        free(id);
        if (!match) {
            continue;
        }

        return get_config_array_item_field(AVAR_CFG_DM_ITEMS, i, AVAR_FIELD_STATUS);
    }

    return NULL;
}

static bool wait_for_item_status(const char *item_id, const char *expected,
                                 const unsigned timeout_ms) {
    const unsigned step_ms = 50U;
    for (unsigned elapsed = 0U; elapsed < timeout_ms; elapsed += step_ms) {
        char *status = find_item_status(item_id);
        if (status != NULL && strcmp(status, expected) == 0) {
            free(status);
            return true;
        }
        free(status);
#if defined(_WIN32)
        Sleep(step_ms);
#else
        usleep((useconds_t)step_ms * 1000U);
#endif
    }
    return false;
}

AVAR_TEST(download_lifecycle_enqueue_and_remove) {
    AVAR_ASSERT(setup_paths());

    char url[256];
    build_url("plain.txt", url, sizeof url);

    AVAR_ASSERT_EQ(transient_download(url, NULL, "lifecycle-test", NULL, true), EXIT_SUCCESS);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 1);

    char *item_id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_ID);
    AVAR_ASSERT_NOT_NULL(item_id);

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
    AVAR_ASSERT(setup_paths());
    thread_pool_reset_global();

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
    thread_pool_reset_global();

    AVAR_ASSERT_EQ(download_remove(item_id, true, true, false), EXIT_SUCCESS);
    free(item_id);
}

AVAR_TEST(download_lifecycle_enqueue_proxy_and_active_list) {
    AVAR_ASSERT(setup_paths());
    thread_pool_reset_global();

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

    thread_pool_reset_global();

    AVAR_ASSERT_EQ(download_remove(item_id, true, true, false), EXIT_SUCCESS);
    free(item_id);
}

AVAR_TEST(download_lifecycle_start_stop_background) {
    AVAR_ASSERT(setup_paths());
    thread_pool_reset_global();

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
    thread_pool_reset_global();

    AVAR_ASSERT_EQ(download_remove(item_id, true, true, true), EXIT_SUCCESS);
    free(item_id);
}

AVAR_TEST(download_lifecycle_resume_interrupted) {
    AVAR_ASSERT(setup_paths());
    thread_pool_reset_global();

    char url[256];
    build_url("plain.txt", url, sizeof url);

    char item_json[512];
    snprintf(item_json, sizeof item_json,
             "{\"" AVAR_FIELD_ID "\":\"dl-resume\",\""
             AVAR_FIELD_URL "\":\"%s\",\""
             AVAR_FIELD_FILENAME "\":\"plain.txt\",\""
             AVAR_FIELD_STATUS "\":\"" AVAR_DL_STATUS_DOWNLOADING "\"}",
             url);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS, item_json), 0);

    download_resume_interrupted();
    AVAR_ASSERT(wait_for_item_status("dl-resume", AVAR_DL_STATUS_COMPLETED, 60000U));
    AVAR_ASSERT(download_wait_idle(60000U));

    thread_pool_reset_global();
    AVAR_ASSERT_EQ(download_remove("dl-resume", true, true, false), EXIT_SUCCESS);
}

AVAR_TEST(download_lifecycle_resume_interrupted_started_queue) {
    AVAR_ASSERT(setup_paths());
    thread_pool_reset_global();

    char *queue_id = NULL;
    AVAR_ASSERT_EQ(queue_add("resume-q", NULL, &queue_id), QueueErrorNone);
    AVAR_ASSERT_NOT_NULL(queue_id);
    AVAR_ASSERT_EQ(queue_start(queue_id), QueueErrorNone);

    char url[256];
    build_url("plain.txt", url, sizeof url);

    char item_json[768];
    snprintf(item_json, sizeof item_json,
             "{\"" AVAR_FIELD_ID "\":\"dl-q-resume\",\""
             AVAR_FIELD_URL "\":\"%s\",\""
             AVAR_FIELD_FILENAME "\":\"plain.txt\",\""
             AVAR_FIELD_QUEUE_ID "\":\"%s\",\""
             AVAR_FIELD_STATUS "\":\"" AVAR_DL_STATUS_QUEUED "\"}",
             url, queue_id);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS, item_json), 0);

    download_resume_interrupted();
    AVAR_ASSERT(wait_for_item_status("dl-q-resume", AVAR_DL_STATUS_COMPLETED, 60000U));
    AVAR_ASSERT(download_wait_idle(60000U));

    thread_pool_reset_global();
    AVAR_ASSERT_EQ(download_remove("dl-q-resume", true, true, false), EXIT_SUCCESS);
    AVAR_ASSERT_EQ(queue_remove(queue_id, false, false), QueueErrorNone);
    free(queue_id);
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
        run_download_lifecycle_resume_interrupted();
        run_download_lifecycle_resume_interrupted_started_queue();
        run_download_lifecycle_invalid_operations();
        test_guard_http_server_stop(&g_http_server);)
