#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "download.h"
#include "download_segment.h"
#include "file-system.h"
#include "thread_pool.h"

#define SEGMENTED_SIZE (512U * 1024U)
#define INTEGRATION_IDLE_WAIT_MS 15000U

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
static char g_segmented_url[256];
static bool g_fixture_ready = false;

static void configure_segmentation(const char *min_file_size, const char *chunk_size,
                                   const char *concurrency, const char *enabled) {
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_ENABLED, enabled), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_MIN_FILE_SIZE, min_file_size), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_CHUNK_SIZE, chunk_size), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_CONCURRENCY, concurrency), 0);
}

static bool read_range_stats(unsigned *max_concurrent, unsigned *total_range_requests) {
    FILE *file = fopen(g_guard.stats_path, "rb");
    if (file == NULL) {
        return false;
    }

    char buffer[128] = {0};
    const size_t read = fread(buffer, 1, sizeof buffer - 1U, file);
    fclose(file);
    if (read == 0U) {
        return false;
    }

    unsigned max_value = 0U;
    unsigned total_value = 0U;
    const char *max_key = strstr(buffer, "\"max_concurrent\":");
    const char *total_key = strstr(buffer, "\"total_range_requests\":");
    if (max_key == NULL || total_key == NULL) {
        return false;
    }

    if (sscanf(max_key, "\"max_concurrent\":%u", &max_value) != 1
        || sscanf(total_key, "\"total_range_requests\":%u", &total_value) != 1) {
        return false;
    }

    if (max_concurrent != NULL) {
        *max_concurrent = max_value;
    }
    if (total_range_requests != NULL) {
        *total_range_requests = total_value;
    }

    return true;
}

static void remove_named_artifacts(const char *name) {
    char *dest = path_join(g_download_dir, name);
    char *temp = path_join(g_temp_dir, name);
    if (dest != NULL) {
        remove(dest);
        free(dest);
    }
    if (temp != NULL) {
        remove(temp);
        free(temp);
    }
}

static void remove_segmented_artifacts(void) {
    remove_named_artifacts("segmented.bin");
}

static bool verify_segmented_file_contents(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < SEGMENTED_SIZE; i++) {
        const int byte = fgetc(file);
        if (byte < 0 || (unsigned char)byte != (unsigned char)(i % 256U)) {
            ok = false;
            break;
        }
    }

    if (ok && fgetc(file) != EOF) {
        ok = false;
    }

    fclose(file);
    return ok;
}

static void setup_isolated_paths(void) {
    if (!g_fixture_ready) {
        AVAR_ASSERT(test_guard_init(&g_guard, "avar-dl-test"));
        snprintf(g_server_script, sizeof g_server_script, "%s/scripts/http_test_server.py",
                 AVAR_SOURCE_DIR);
        test_guard_http_server_init(&g_http_server);
        AVAR_ASSERT(test_guard_http_server_start(&g_guard, g_server_script, &g_http_server));
        AVAR_ASSERT(test_guard_http_url(&g_guard, "segmented.bin", g_segmented_url,
                                        sizeof g_segmented_url));
        g_fixture_ready = true;
    }

    AVAR_ASSERT(test_guard_http_server_reset_stats(&g_guard));

    snprintf(g_temp_dir, sizeof g_temp_dir, "%s%ctemp", g_guard.work_dir, PATH_SEPARATOR);
    snprintf(g_download_dir, sizeof g_download_dir, "%s%cdownload", g_guard.work_dir,
             PATH_SEPARATOR);

    (void)make_dirs_in_path(g_temp_dir);
    (void)make_dirs_in_path(g_download_dir);

    remove(g_guard.config_path);

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

    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_TEMP_PATH, g_temp_dir), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_DOWNLOAD_PATH, g_download_dir), 0);
}

static void build_url(const char *path, char *out, size_t out_size) {
    AVAR_ASSERT(test_guard_http_url(&g_guard, path, out, out_size));
}

AVAR_TEST(download_integration_redirect_follow) {
    setup_isolated_paths();

    char url[256];
    build_url("redirect.bin", url, sizeof url);
    const int rc = transient_download(url, NULL, NULL, NULL, true);

    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);

    char *dest = path_join(g_download_dir, "plain.txt");
    AVAR_ASSERT_NOT_NULL(dest);
    AVAR_ASSERT(file_exists(dest));
    free(dest);
}

AVAR_TEST(download_integration_attached_roundtrip) {
    setup_isolated_paths();

    char url[256];
    build_url("plain.txt", url, sizeof url);
    const int rc = transient_download(url, NULL, NULL, NULL, true);

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

AVAR_TEST(download_integration_small_file_skips_segmentation) {
    setup_isolated_paths();
    configure_segmentation("1048576", "65536", "4", "true");

    char url[256];
    build_url("plain.txt", url, sizeof url);
    const int rc = transient_download(url, NULL, NULL, NULL, true);

    unsigned max_concurrent = 0U;
    unsigned total_range_requests = 0U;
    AVAR_ASSERT(read_range_stats(&max_concurrent, &total_range_requests));

    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);
    AVAR_ASSERT_EQ(max_concurrent, 0U);
    AVAR_ASSERT_EQ(total_range_requests, 0U);
}

AVAR_TEST(download_integration_segmented_parallel_completes) {
    setup_isolated_paths();
    configure_segmentation("1024", "65536", "4", "true");
    remove_segmented_artifacts();

    const int rc = transient_download(g_segmented_url, NULL, NULL, NULL, true);

    unsigned max_concurrent = 0U;
    unsigned total_range_requests = 0U;
    AVAR_ASSERT(read_range_stats(&max_concurrent, &total_range_requests));

    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);
    AVAR_ASSERT(max_concurrent >= 2U);
    AVAR_ASSERT(total_range_requests >= 4U);

    char *dest = path_join(g_download_dir, "segmented.bin");
    AVAR_ASSERT_NOT_NULL(dest);
    AVAR_ASSERT(verify_segmented_file_contents(dest));
    free(dest);
}

AVAR_TEST(download_integration_segmented_disabled_uses_stream) {
    setup_isolated_paths();
    configure_segmentation("1024", "65536", "4", "false");
    remove_segmented_artifacts();

    const int rc = transient_download(g_segmented_url, NULL, NULL, NULL, true);

    unsigned max_concurrent = 0U;
    unsigned total_range_requests = 0U;
    AVAR_ASSERT(read_range_stats(&max_concurrent, &total_range_requests));

    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);
    AVAR_ASSERT_EQ(max_concurrent, 0U);
    AVAR_ASSERT_EQ(total_range_requests, 0U);

    char *dest = path_join(g_download_dir, "segmented.bin");
    AVAR_ASSERT_NOT_NULL(dest);
    AVAR_ASSERT(verify_segmented_file_contents(dest));
    free(dest);
}

AVAR_TEST(download_integration_segment_retry_recovers_from_drop) {
    setup_isolated_paths();
    configure_segmentation("1024", "65536", "4", "true");
    remove_named_artifacts("flaky_segmented.bin");

    char url[256];
    build_url("flaky_segmented.bin", url, sizeof url);
    const int rc = transient_download(url, NULL, NULL, NULL, true);

    unsigned total_range_requests = 0U;
    AVAR_ASSERT(read_range_stats(NULL, &total_range_requests));

    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);

    char *dest = path_join(g_download_dir, "flaky_segmented.bin");
    AVAR_ASSERT_NOT_NULL(dest);
    AVAR_ASSERT(verify_segmented_file_contents(dest));
    free(dest);
}

AVAR_TEST(download_integration_range_refused_falls_back_to_stream) {
    setup_isolated_paths();
    configure_segmentation("1024", "65536", "4", "true");
    remove_named_artifacts("liesrange.bin");

    char url[256];
    build_url("liesrange.bin", url, sizeof url);
    const int rc = transient_download(url, NULL, NULL, NULL, true);

    AVAR_ASSERT_EQ(rc, EXIT_SUCCESS);

    char *dest = path_join(g_download_dir, "liesrange.bin");
    AVAR_ASSERT_NOT_NULL(dest);
    AVAR_ASSERT(verify_segmented_file_contents(dest));
    free(dest);
}

AVAR_TEST(download_integration_background_downloads_use_thread_pool) {
    setup_isolated_paths();
    configure_segmentation("1024", "65536", "4", "true");
    remove_segmented_artifacts();
    thread_pool_reset_global();

    char url1[256];
    char url2[256];
    char url3[256];
    build_url("segmented.bin", url1, sizeof url1);
    build_url("segmented2.bin", url2, sizeof url2);
    build_url("segmented3.bin", url3, sizeof url3);

    AVAR_ASSERT_EQ(download_start_background(url1, NULL, NULL, NULL), EXIT_SUCCESS);
    AVAR_ASSERT_EQ(download_start_background(url2, NULL, NULL, NULL), EXIT_SUCCESS);
    AVAR_ASSERT_EQ(download_start_background(url3, NULL, NULL, NULL), EXIT_SUCCESS);

    size_t peak_active = 0U;
    for (int i = 0; i < 100; i++) {
        const size_t active = download_active_count();
        if (active > peak_active) {
            peak_active = active;
        }
#if defined(_WIN32)
        Sleep(20);
#else
        usleep(20000);
#endif
    }

    AVAR_ASSERT(peak_active >= 1U);
    AVAR_ASSERT(download_wait_idle(INTEGRATION_IDLE_WAIT_MS));

    thread_pool_reset_global();
}

AVAR_TEST_MAIN(
        run_download_integration_attached_roundtrip();
        run_download_integration_redirect_follow();
        run_download_integration_small_file_skips_segmentation();
        run_download_integration_segmented_parallel_completes();
        run_download_integration_segmented_disabled_uses_stream();
        run_download_integration_segment_retry_recovers_from_drop();
        run_download_integration_range_refused_falls_back_to_stream();
        run_download_integration_background_downloads_use_thread_pool();
        test_guard_http_server_stop(&g_http_server);)
