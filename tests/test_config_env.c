#include "avar_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "file-system.h"

#if defined(_WIN32)
    #include <stdlib.h>
#else
    #include <unistd.h>
#endif

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

    snprintf(g_config_path, sizeof g_config_path, "%s%cavar-test-config-env.json", base,
             PATH_SEPARATOR);
    remove(g_config_path);
    AVAR_ASSERT_EQ(config_open_at(g_config_path), 0);
}

static void set_test_env(const char *key, const char *value) {
    char name[AVAR_CONFIG_PATH_MAX];
    snprintf(name, sizeof name, "%s%s", AVAR_ENV_PREFIX, key);
#if defined(_WIN32)
    char entry[AVAR_CONFIG_PATH_MAX];
    snprintf(entry, sizeof entry, "%s=%s", name, value);
    AVAR_ASSERT_EQ(_putenv(entry), 0);
#else
    AVAR_ASSERT_EQ(setenv(name, value, 1), 0);
#endif
}

static void unset_test_env(const char *key) {
    char name[AVAR_CONFIG_PATH_MAX];
    snprintf(name, sizeof name, "%s%s", AVAR_ENV_PREFIX, key);
#if defined(_WIN32)
    char entry[AVAR_CONFIG_PATH_MAX];
    snprintf(entry, sizeof entry, "%s=", name);
    AVAR_ASSERT_EQ(_putenv(entry), 0);
#else
    AVAR_ASSERT_EQ(unsetenv(name), 0);
#endif
}

AVAR_TEST(config_env_overrides_file_value) {
    setup_temp_config();

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_LOCAL), 0);

    set_test_env(AVAR_CFG_DAEMON_SESSION_MODE, AVAR_DAEMON_SESSION_MODE_REMOTE);

    char *value = get_config(AVAR_CFG_DAEMON_SESSION_MODE);
    AVAR_ASSERT_NOT_NULL(value);
    AVAR_ASSERT_STR_EQ(value, AVAR_DAEMON_SESSION_MODE_REMOTE);
    free(value);

    unset_test_env(AVAR_CFG_DAEMON_SESSION_MODE);

    value = get_config(AVAR_CFG_DAEMON_SESSION_MODE);
    AVAR_ASSERT_NOT_NULL(value);
    AVAR_ASSERT_STR_EQ(value, AVAR_DAEMON_SESSION_MODE_LOCAL);
    free(value);
}

AVAR_TEST(config_env_overrides_default) {
    setup_temp_config();

    set_test_env(AVAR_CFG_DAEMON_CHANNELS_HTTP_PORT, "9001");

    char *value = get_config_or_default(AVAR_CFG_DAEMON_CHANNELS_HTTP_PORT, "8000");
    AVAR_ASSERT_NOT_NULL(value);
    AVAR_ASSERT_STR_EQ(value, "9001");
    free(value);

    unset_test_env(AVAR_CFG_DAEMON_CHANNELS_HTTP_PORT);
}

AVAR_TEST_MAIN(
        run_config_env_overrides_file_value();
        run_config_env_overrides_default();)
