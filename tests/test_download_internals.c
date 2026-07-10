#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "download.h"
#include "config.h"
#include "file-system.h"
#include "queue.h"
#include "download_state.h"

static TestGuard g_guard;

AVAR_TEST(download_internals_choose_filename) {
    char *from_url = download_test_choose_filename("http://example.com/path/file.bin", NULL, 0U);
    AVAR_ASSERT_NOT_NULL(from_url);
    AVAR_ASSERT_STR_EQ(from_url, "file.bin");
    free(from_url);

    const char *cd = "attachment; filename=\"report.pdf\"";
    char *from_header = download_test_choose_filename("http://example.com/x", cd, strlen(cd));
    AVAR_ASSERT_NOT_NULL(from_header);
    AVAR_ASSERT_STR_EQ(from_header, "report.pdf");
    free(from_header);
}

AVAR_TEST(download_internals_parse_content_range) {
    uint64_t total = 0U;
    AVAR_ASSERT(download_test_parse_content_range_total("bytes 0-99/1000", &total));
    AVAR_ASSERT_EQ(total, 1000U);

    AVAR_ASSERT(!download_test_parse_content_range_total("invalid", &total));
    AVAR_ASSERT(!download_test_parse_content_range_total(NULL, &total));
}

AVAR_TEST(download_internals_existing_file_size) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-dl-internals"));
    (void)make_dirs_in_path(g_guard.work_dir);

    char path[512];
    snprintf(path, sizeof path, "%s%csample.txt", g_guard.work_dir, PATH_SEPARATOR);
    FILE *file = fopen(path, "wb");
    AVAR_ASSERT_NOT_NULL(file);
    AVAR_ASSERT(fwrite("hello", 1U, 5U, file) == 5U);
    fclose(file);

    AVAR_ASSERT_EQ(download_test_existing_file_size(path), 5U);
    AVAR_ASSERT_EQ(download_test_existing_file_size("missing-file-xyz"), 0U);
    remove(path);
}

AVAR_TEST(download_internals_generate_id) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-dl-internals-id"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);

    char *id1 = download_test_generate_id();
    char *id2 = download_test_generate_id();
    AVAR_ASSERT_NOT_NULL(id1);
    AVAR_ASSERT_NOT_NULL(id2);
    AVAR_ASSERT(strncmp(id1, AVAR_DL_ID_PREFIX, strlen(AVAR_DL_ID_PREFIX)) == 0);
    AVAR_ASSERT(strcmp(id1, id2) != 0);
    free(id1);
    free(id2);
}

AVAR_TEST(download_internals_progress_watch) {
    AVAR_ASSERT(!download_progress_is_watched("dl-test-watch"));
    download_progress_watch("dl-test-watch");
    AVAR_ASSERT(download_progress_is_watched("dl-test-watch"));
    download_progress_watch("dl-test-watch");
    AVAR_ASSERT(download_progress_is_watched("dl-test-watch"));
    download_progress_unwatch("dl-test-watch");
    AVAR_ASSERT(download_progress_is_watched("dl-test-watch"));
    download_progress_unwatch("dl-test-watch");
    AVAR_ASSERT(!download_progress_is_watched("dl-test-watch"));
    download_progress_unwatch("dl-test-watch");
}

AVAR_TEST(download_internals_status_validation) {
    AVAR_ASSERT(download_test_status_is_valid(AVAR_DL_STATUS_QUEUED));
    AVAR_ASSERT(download_test_status_is_valid(AVAR_DL_STATUS_DOWNLOADING));
    AVAR_ASSERT(download_test_status_is_valid(AVAR_DL_STATUS_COMPLETED));
    AVAR_ASSERT(!download_test_status_is_valid(NULL));
    AVAR_ASSERT(!download_test_status_is_valid(""));
    AVAR_ASSERT(!download_test_status_is_valid("dl-9fb4ee1a165da675"));
    AVAR_ASSERT(!download_test_status_is_valid("mystery"));
}

AVAR_TEST(download_internals_set_queue_to_default) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-dl-internals-set-queue"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);

    char temp_dir[512];
    snprintf(temp_dir, sizeof temp_dir, "%s%ctemp", g_guard.work_dir, PATH_SEPARATOR);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_TEMP_PATH, temp_dir), 0);
    AVAR_ASSERT_EQ(make_dirs_in_path(temp_dir), 0);

    char *queue_id = NULL;
    AVAR_ASSERT_EQ(queue_add("assigned-q", NULL, &queue_id), QueueErrorNone);
    AVAR_ASSERT_NOT_NULL(queue_id);

    const char *item_id = "dl-set-queue-default";
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"dl-set-queue-default\","
                                            "\"" AVAR_FIELD_FILENAME "\":\"f.bin\","
                                            "\"" AVAR_FIELD_STATUS "\":\"" AVAR_DL_STATUS_QUEUED
                                            "\",\"" AVAR_FIELD_QUEUE_ID "\":null}"),
                     0);

    char job_dir[512];
    snprintf(job_dir, sizeof job_dir, "%s%c%s", temp_dir, PATH_SEPARATOR, item_id);
    AVAR_ASSERT_EQ(make_dirs_in_path(job_dir), 0);

    char state_path[640];
    snprintf(state_path, sizeof state_path, "%s%cstate.json", job_dir, PATH_SEPARATOR);

    char state_json[768];
    snprintf(state_json, sizeof state_json,
             "{\"" AVAR_FIELD_ID "\":\"%s\",\"" AVAR_FIELD_URL
             "\":\"http://example.com/f.bin\",\"" AVAR_FIELD_FILENAME
             "\":\"f.bin\",\"dest_path\":\"/tmp/f.bin\",\"temp_path\":\"/tmp/f.bin\",\""
             AVAR_FIELD_QUEUE_ID "\":\"%s\",\"" AVAR_FIELD_STATUS
             "\":\"" AVAR_DL_STATUS_QUEUED "\"}",
             item_id, queue_id);
    FILE *file = fopen(state_path, "wb");
    AVAR_ASSERT_NOT_NULL(file);
    AVAR_ASSERT_EQ(fwrite(state_json, 1U, strlen(state_json), file), strlen(state_json));
    fclose(file);

    AVAR_ASSERT_EQ(download_set_queue(item_id, NULL), EXIT_SUCCESS);

    char *config_queue = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_QUEUE_ID);
    AVAR_ASSERT(config_queue == NULL || config_queue[0] == '\0');
    free(config_queue);

    DownloadState *state = download_item_state_load(item_id);
    AVAR_ASSERT_NOT_NULL(state);
    AVAR_ASSERT(state->queue_id == NULL || state->queue_id[0] == '\0');
    download_state_free(state);
    free(queue_id);
}

AVAR_TEST_MAIN(
        run_download_internals_choose_filename();
        run_download_internals_parse_content_range();
        run_download_internals_existing_file_size();
        run_download_internals_generate_id();
        run_download_internals_progress_watch();
        run_download_internals_status_validation();
        run_download_internals_set_queue_to_default();)
