#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>

#include "avar.h"
#include "config.h"
#include "file-system.h"
#include "logger.h"
#include "queue.h"

static TestGuard g_guard;

static void setup_temp_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-queue"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
    init_logger(false);
}

AVAR_TEST(queue_add_and_list) {
    setup_temp_config();

    char *id = NULL;
    QueueOptions options = {.description = "primary queue", .max_concurrent_downloads = 2};
    AVAR_ASSERT_EQ(queue_add("default", &options, &id), QueueErrorNone);
    AVAR_ASSERT_NOT_NULL(id);
    AVAR_ASSERT_EQ(queue_count(), 1);

    char *name = queue_name_at(0);
    AVAR_ASSERT_NOT_NULL(name);
    AVAR_ASSERT_STR_EQ(name, "default");
    free(name);
    free(id);
}

AVAR_TEST(queue_edit_and_resolve) {
    setup_temp_config();

    char *id = NULL;
    AVAR_ASSERT_EQ(queue_add("work", NULL, &id), QueueErrorNone);
    AVAR_ASSERT_NOT_NULL(id);

    QueuePatch patch = {.set_description = true,
                        .description = "updated",
                        .set_temp_path = true,
                        .temp_path = "C:\\temp"};
    AVAR_ASSERT_EQ(queue_edit(id, &patch), QueueErrorNone);

    char *resolved = queue_resolve_id("work", true);
    AVAR_ASSERT_NOT_NULL(resolved);
    AVAR_ASSERT_STR_EQ(resolved, id);
    free(resolved);

    char *description =
        get_config_array_item_field(AVAR_CFG_DM_QUEUES, 0, AVAR_FIELD_DESCRIPTION);
    AVAR_ASSERT_NOT_NULL(description);
    AVAR_ASSERT_STR_EQ(description, "updated");
    free(description);

    free(id);
}

AVAR_TEST(queue_remove_detaches_items) {
    setup_temp_config();

    char *id = NULL;
    AVAR_ASSERT_EQ(queue_add("batch", NULL, &id), QueueErrorNone);
    AVAR_ASSERT_NOT_NULL(id);

    char item_json[256];
    snprintf(item_json, sizeof item_json,
             "{\"" AVAR_FIELD_ID "\":\"dl-1\",\"" AVAR_FIELD_URL
             "\":\"https://example.com\",\"" AVAR_FIELD_QUEUE_ID "\":\"%s\"}",
             id);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS, item_json), 0);

    AVAR_ASSERT_EQ(queue_remove(id, false, false), QueueErrorNone);
    AVAR_ASSERT_EQ(queue_count(), 0);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 1);

    char *queue_id = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_QUEUE_ID);
    AVAR_ASSERT_NULL(queue_id);

    free(id);
}

AVAR_TEST(queue_remove_purges_items) {
    setup_temp_config();

    char *id = NULL;
    AVAR_ASSERT_EQ(queue_add("purge-me", NULL, &id), QueueErrorNone);
    AVAR_ASSERT_NOT_NULL(id);

    char item_json[256];
    snprintf(item_json, sizeof item_json,
             "{\"" AVAR_FIELD_ID "\":\"dl-2\",\"" AVAR_FIELD_URL
             "\":\"https://example.com\",\"" AVAR_FIELD_QUEUE_ID "\":\"%s\"}",
             id);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS, item_json), 0);

    AVAR_ASSERT_EQ(queue_remove(id, false, true), QueueErrorNone);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 0);

    free(id);
}

AVAR_TEST(queue_stop_resets_downloading_items) {
    setup_temp_config();

    char *id = NULL;
    AVAR_ASSERT_EQ(queue_add("stop-me", NULL, &id), QueueErrorNone);
    AVAR_ASSERT_NOT_NULL(id);

    char item_json[256];
    snprintf(item_json, sizeof item_json,
             "{\"" AVAR_FIELD_ID "\":\"dl-3\",\"" AVAR_FIELD_URL
             "\":\"https://example.com\",\"" AVAR_FIELD_QUEUE_ID "\":\"%s\",\""
             AVAR_FIELD_STATUS "\":\"" AVAR_DL_STATUS_DOWNLOADING "\"}",
             id);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS, item_json), 0);

    AVAR_ASSERT_EQ(queue_stop(id), QueueErrorNone);

    char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_STATUS);
    AVAR_ASSERT_NOT_NULL(status);
    AVAR_ASSERT_STR_EQ(status, AVAR_DL_STATUS_QUEUED);
    free(status);

    free(id);
}

AVAR_TEST_MAIN(
        run_queue_add_and_list();
        run_queue_edit_and_resolve();
        run_queue_remove_detaches_items();
        run_queue_remove_purges_items();
        run_queue_stop_resets_downloading_items();)
