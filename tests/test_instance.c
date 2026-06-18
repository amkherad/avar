#include "avar_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "instance.h"

#if defined(_WIN32)
    #include <stdlib.h>
#else
    #include <unistd.h>
#endif

static void set_instance_env(const char *value) {
#if defined(_WIN32)
    char entry[128];
    if (value == NULL) {
        snprintf(entry, sizeof entry, "%s=", AVAR_ENV_INSTANCE);
    } else {
        snprintf(entry, sizeof entry, "%s=%s", AVAR_ENV_INSTANCE, value);
    }
    AVAR_ASSERT_EQ(_putenv(entry), 0);
#else
    if (value == NULL) {
        AVAR_ASSERT_EQ(unsetenv(AVAR_ENV_INSTANCE), 0);
    } else {
        AVAR_ASSERT_EQ(setenv(AVAR_ENV_INSTANCE, value, 1), 0);
    }
#endif
}

AVAR_TEST(instance_defaults_without_env) {
    set_instance_env(NULL);

    char id[AVAR_INSTANCE_ID_MAX + 1U];
    AVAR_ASSERT(!avar_instance_configured());
    AVAR_ASSERT(!avar_instance_id(id, sizeof id));
    AVAR_ASSERT_STR_EQ(avar_app_name(), APP_ID);
    AVAR_ASSERT_EQ(avar_instance_port_offset(), 0U);
}

AVAR_TEST(instance_suffixes_paths_and_resources) {
    set_instance_env("work");

    char path[AVAR_CONFIG_PATH_MAX];
    AVAR_ASSERT(avar_path_with_instance(path, sizeof path, "/tmp/avar.pid"));
    AVAR_ASSERT_STR_EQ(path, "/tmp/avar-work.pid");

#if defined(_WIN32)
    AVAR_ASSERT(avar_path_with_instance(path, sizeof path, "\\\\.\\pipe\\avar"));
    AVAR_ASSERT_STR_EQ(path, "\\\\.\\pipe\\avar-work");
#endif

    AVAR_ASSERT(avar_path_with_instance(path, sizeof path, "/tmp/avar.sock"));
    AVAR_ASSERT_STR_EQ(path, "/tmp/avar-work.sock");

    AVAR_ASSERT_STR_EQ(avar_app_name(), "avar-work");
    AVAR_ASSERT(avar_instance_port_offset() > 0U);

    set_instance_env(NULL);
}

AVAR_TEST_MAIN(
        run_instance_defaults_without_env();
        run_instance_suffixes_paths_and_resources();)
