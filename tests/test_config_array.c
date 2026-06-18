#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>

#include "avar.h"
#include "config.h"
#include "file-system.h"

static TestGuard g_guard;

static void setup_temp_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-config"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
}

AVAR_TEST(config_array_append_and_query) {
    setup_temp_config();

    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"1\",\"" AVAR_FIELD_URL
                                            "\":\"https://a\"}"),
                   0);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"2\",\"" AVAR_FIELD_URL
                                            "\":\"https://b\"}"),
                   0);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 2);

    char *url = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 1, AVAR_FIELD_URL);
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "https://b");
    free(url);
}

AVAR_TEST(config_array_update_and_remove) {
    setup_temp_config();

    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_ITEMS,
                                            "{\"" AVAR_FIELD_ID "\":\"x\",\"" AVAR_FIELD_STATUS
                                            "\":\"" AVAR_DL_STATUS_QUEUED "\"}"),
                   0);
    AVAR_ASSERT_EQ(update_config_array_item(AVAR_CFG_DM_ITEMS, AVAR_FIELD_ID, "x",
                                           "{\"" AVAR_FIELD_ID "\":\"x\",\"" AVAR_FIELD_STATUS
                                           "\":\"done\"}"),
                   0);

    char *status = get_config_array_item_field(AVAR_CFG_DM_ITEMS, 0, AVAR_FIELD_STATUS);
    AVAR_ASSERT_NOT_NULL(status);
    AVAR_ASSERT_STR_EQ(status, "done");
    free(status);

    AVAR_ASSERT_EQ(remove_config_array_item(AVAR_CFG_DM_ITEMS, AVAR_FIELD_ID, "x"), 0);
    AVAR_ASSERT_EQ(get_config_array_size(AVAR_CFG_DM_ITEMS), 0);
}

AVAR_TEST_MAIN(
        run_config_array_append_and_query();
        run_config_array_update_and_remove();)
