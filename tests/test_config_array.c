#include "avar_test.h"

#include <stdio.h>

#include "config.h"
#include "file-system.h"

static char g_config_path[512];

static void setup_temp_config(void) {
#if defined(_WIN32)
    const char *base = getenv("TEMP");
#else
    const char *base = "/tmp";
#endif
    if (base == NULL) {
        base = ".";
    }

    snprintf(g_config_path, sizeof g_config_path, "%s%cavar-test-config.json", base, PATH_SEPARATOR);
    remove(g_config_path);
    AVAR_ASSERT_EQ(config_open_at(g_config_path), 0);
}

AVAR_TEST(config_array_append_and_query) {
    setup_temp_config();

    AVAR_ASSERT_EQ(append_config_array_item("dm.items", "{\"id\":\"1\",\"url\":\"https://a\"}"), 0);
    AVAR_ASSERT_EQ(append_config_array_item("dm.items", "{\"id\":\"2\",\"url\":\"https://b\"}"), 0);
    AVAR_ASSERT_EQ(get_config_array_size("dm.items"), 2);

    char *url = get_config_array_item_field("dm.items", 1, "url");
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "https://b");
    free(url);
}

AVAR_TEST(config_array_update_and_remove) {
    setup_temp_config();

    AVAR_ASSERT_EQ(append_config_array_item("dm.items", "{\"id\":\"x\",\"status\":\"queued\"}"), 0);
    AVAR_ASSERT_EQ(
            update_config_array_item("dm.items", "id", "x", "{\"id\":\"x\",\"status\":\"done\"}"),
            0);

    char *status = get_config_array_item_field("dm.items", 0, "status");
    AVAR_ASSERT_NOT_NULL(status);
    AVAR_ASSERT_STR_EQ(status, "done");
    free(status);

    AVAR_ASSERT_EQ(remove_config_array_item("dm.items", "id", "x"), 0);
    AVAR_ASSERT_EQ(get_config_array_size("dm.items"), 0);
}

AVAR_TEST_MAIN(
        run_config_array_append_and_query();
        run_config_array_update_and_remove();)
