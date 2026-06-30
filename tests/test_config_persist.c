#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "file-system.h"

static TestGuard g_guard;

static void setup_temp_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-persist"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
}

AVAR_TEST(config_save_and_load_roundtrip) {
    setup_temp_config();

    char save_path[512];
    snprintf(save_path, sizeof save_path, "%s%csaved-config.json", g_guard.work_dir, PATH_SEPARATOR);

    AVAR_ASSERT_EQ(set_config("download.test.key", "value"), 0);
    AVAR_ASSERT_EQ(save_config(save_path), 0);

    AVAR_ASSERT_EQ(reset_all_config(), 0);
    AVAR_ASSERT_NULL(get_config("download.test.key"));

    AVAR_ASSERT_EQ(load_config(save_path), 0);
    char *value = get_config("download.test.key");
    AVAR_ASSERT_NOT_NULL(value);
    AVAR_ASSERT_STR_EQ(value, "value");
    free(value);
}

AVAR_TEST(config_reset_key) {
    setup_temp_config();

    AVAR_ASSERT_EQ(set_config("download.test.key", "value"), 0);
    AVAR_ASSERT_EQ(reset_config("download.test.key"), 0);
    AVAR_ASSERT_NULL(get_config("download.test.key"));
}

AVAR_TEST(config_get_or_default) {
    setup_temp_config();

    char *missing = get_config_or_default("missing.key", "fallback");
    AVAR_ASSERT_NOT_NULL(missing);
    AVAR_ASSERT_STR_EQ(missing, "fallback");
    free(missing);

    AVAR_ASSERT_EQ(set_config("present.key", "actual"), 0);
    char *present = get_config_or_default("present.key", "fallback");
    AVAR_ASSERT_NOT_NULL(present);
    AVAR_ASSERT_STR_EQ(present, "actual");
    free(present);
}

AVAR_TEST(config_get_scalar_types) {
    setup_temp_config();

    char typed_path[512];
    snprintf(typed_path, sizeof typed_path, "%s%ctyped-config.json", g_guard.work_dir,
             PATH_SEPARATOR);

    FILE *file = fopen(typed_path, "wb");
    AVAR_ASSERT_NOT_NULL(file);
    fputs("{\"dm\":{\"segmentation\":{\"concurrency\":8,\"enabled\":true,\"strategy\":\"left-heavy\"}}}",
          file);
    fclose(file);

    AVAR_ASSERT_EQ(load_config(typed_path), 0);

    char *concurrency = get_config("dm.segmentation.concurrency");
    AVAR_ASSERT_NOT_NULL(concurrency);
    AVAR_ASSERT_STR_EQ(concurrency, "8");
    free(concurrency);

    char *enabled = get_config("dm.segmentation.enabled");
    AVAR_ASSERT_NOT_NULL(enabled);
    AVAR_ASSERT_STR_EQ(enabled, "true");
    free(enabled);

    char *strategy = get_config("dm.segmentation.strategy");
    AVAR_ASSERT_NOT_NULL(strategy);
    AVAR_ASSERT_STR_EQ(strategy, "left-heavy");
    free(strategy);
}

AVAR_TEST(config_set_scalar_roundtrip) {
    setup_temp_config();

    AVAR_ASSERT_EQ(set_config("dm.segmentation.concurrency", "12"), 0);
    char *concurrency = get_config("dm.segmentation.concurrency");
    AVAR_ASSERT_NOT_NULL(concurrency);
    AVAR_ASSERT_STR_EQ(concurrency, "12");
    free(concurrency);

    AVAR_ASSERT_EQ(set_config("dm.segmentation.strategy", "left-heavy"), 0);
    char *strategy = get_config("dm.segmentation.strategy");
    AVAR_ASSERT_NOT_NULL(strategy);
    AVAR_ASSERT_STR_EQ(strategy, "left-heavy");
    free(strategy);

    AVAR_ASSERT_EQ(set_config("dm.segmentation.enabled", "false"), 0);
    char *enabled = get_config("dm.segmentation.enabled");
    AVAR_ASSERT_NOT_NULL(enabled);
    AVAR_ASSERT_STR_EQ(enabled, "false");
    free(enabled);
}

AVAR_TEST(config_array_json_and_replace) {
    setup_temp_config();

    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"1\",\"" AVAR_FIELD_URL
                                            "\":\"https://a\"}"),
                   0);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"2\",\"" AVAR_FIELD_URL
                                            "\":\"https://b\"}"),
                   0);

    char *json = get_config_array_item_json(AVAR_CFG_DM_ITEMS, 1);
    AVAR_ASSERT_NOT_NULL(json);
    AVAR_ASSERT(strstr(json, "https://b") != NULL);
    free(json);

    AVAR_ASSERT_EQ(replace_config_array_item_at(AVAR_CFG_DM_ITEMS, 0,
                                                "{\"" AVAR_FIELD_ID "\":\"1\",\"" AVAR_FIELD_URL
                                                "\":\"https://updated\"}"),
                   0);
    char *url = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_URL);
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "https://updated");
    free(url);
}

AVAR_TEST(config_array_remove_where) {
    setup_temp_config();

    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"a\",\"" AVAR_FIELD_STATUS
                                            "\":\"queued\"}"),
                   0);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"b\",\"" AVAR_FIELD_STATUS
                                            "\":\"queued\"}"),
                   0);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"a\",\"" AVAR_FIELD_STATUS
                                            "\":\"done\"}"),
                   0);

    const int removed =
            remove_config_array_items_where(AVAR_CFG_DM_ITEMS, AVAR_FIELD_ID, "a");
    AVAR_ASSERT_EQ(removed, 2);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 1);
}

AVAR_TEST_MAIN(
        run_config_save_and_load_roundtrip();
        run_config_reset_key();
        run_config_get_or_default();
        run_config_get_scalar_types();
        run_config_set_scalar_roundtrip();
        run_config_array_json_and_replace();
        run_config_array_remove_where();)
