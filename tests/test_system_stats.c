#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "avar.h"
#include "config.h"
#include "file-system.h"
#include "system_stats.h"

static TestGuard g_guard;

static void setup_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-stats"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);

    char download_path[512];
    snprintf(download_path, sizeof download_path, "%s%cdownloads", g_guard.work_dir,
             PATH_SEPARATOR);
    (void)make_dirs_in_path(download_path);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_DOWNLOAD_PATH, download_path), 0);
}

AVAR_TEST(system_stats_collect_populates_fields) {
    setup_config();

    SystemStats stats;
    AVAR_ASSERT_EQ(system_stats_collect(&stats), 0);
    AVAR_ASSERT(stats.disk_total_bytes > 0U);
    AVAR_ASSERT(stats.disk_free_bytes <= stats.disk_total_bytes);
    AVAR_ASSERT(stats.memory_total_bytes > 0U);
    AVAR_ASSERT(stats.memory_used_bytes <= stats.memory_total_bytes);
    AVAR_ASSERT(stats.memory_used_percent >= 0.0);
    AVAR_ASSERT(stats.cpu_usage_percent >= 0.0);
}

AVAR_TEST(system_stats_rejects_null_output) {
    AVAR_ASSERT_EQ(system_stats_collect(NULL), -1);
}

AVAR_TEST(system_stats_cpu_second_sample) {
    setup_config();

    SystemStats first = {0};
    SystemStats second = {0};
    AVAR_ASSERT_EQ(system_stats_collect(&first), 0);
#if defined(_WIN32)
    Sleep(50);
#else
    usleep(50000);
#endif
    AVAR_ASSERT_EQ(system_stats_collect(&second), 0);
    AVAR_ASSERT(second.cpu_usage_percent >= 0.0);
}

AVAR_TEST_MAIN(
        run_system_stats_collect_populates_fields();
        run_system_stats_rejects_null_output();
        run_system_stats_cpu_second_sample();)
