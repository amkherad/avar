#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avar.h"
#include "config.h"
#include "file-system.h"
#include "logger.h"

static TestGuard g_guard;

static void setup_logger_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-logger"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
}

AVAR_TEST(logger_init_and_levels) {
    init_logger(false);
    AVAR_ASSERT_EQ(get_log_level(), LOG_LEVEL_INFO);
    AVAR_ASSERT(is_log_level(LOG_LEVEL_INFO));
    AVAR_ASSERT(!is_log_level(LOG_LEVEL_WARNING));

    init_logger(true);
    AVAR_ASSERT_EQ(get_log_level(), LOG_LEVEL_VERBOSE);
    AVAR_ASSERT(is_log_level(LOG_LEVEL_DEBUG));
}

AVAR_TEST(logger_custom_output_stream) {
    FILE *capture = tmpfile();
    AVAR_ASSERT_NOT_NULL(capture);

    logger_t custom = {.out = capture, .min_lvl = LOG_LEVEL_WARNING};
    set_logger(custom);

    LOG_INFO("hidden");
    LOG_WARNING("visible");

    fflush(capture);
    fseek(capture, 0, SEEK_SET);

    char buffer[256] = {0};
    AVAR_ASSERT(fgets(buffer, sizeof buffer, capture) != NULL);
    AVAR_ASSERT(strstr(buffer, "visible") != NULL);
    AVAR_ASSERT(strstr(buffer, "hidden") == NULL);

    fclose(capture);
    init_logger(false);
}

AVAR_TEST(logger_file_output_from_config) {
    setup_logger_config();

    char log_path[512];
    snprintf(log_path, sizeof log_path, "%s%ctest.log", g_guard.work_dir, PATH_SEPARATOR);

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_LOG_FILE_ENABLED, "true"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_LOG_FILE_PATH, log_path), 0);

    init_logger(false);
    LOG_ERROR("file-log-test");

    logger_close();

    FILE *log = fopen(log_path, "rb");
    AVAR_ASSERT_NOT_NULL(log);
    char line[256] = {0};
    AVAR_ASSERT(fgets(line, sizeof line, log) != NULL);
    AVAR_ASSERT(strstr(line, "file-log-test") != NULL);
    fclose(log);
    remove(log_path);
}

AVAR_TEST(logger_apply_config_disabled) {
    setup_logger_config();
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_LOG_FILE_ENABLED, "false"), 0);

    init_logger(false);
    logger_apply_config();
    LOG_INFO("no-file");
    logger_close();
}

AVAR_TEST_MAIN(
        run_logger_init_and_levels();
        run_logger_custom_output_stream();
        run_logger_file_output_from_config();
        run_logger_apply_config_disabled();)
