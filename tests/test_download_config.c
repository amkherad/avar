#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>

#include "avar.h"
#include "config.h"
#include "download_config.h"

static TestGuard g_guard;

static void setup_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-dlcfg"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
}

AVAR_TEST(download_config_defaults) {
    setup_config();

    const DownloadSegmentConfig cfg = download_config_load(NULL);
    AVAR_ASSERT(cfg.enabled);
    AVAR_ASSERT_EQ(cfg.strategy, SegmentStrategyBalanced);
    AVAR_ASSERT(cfg.concurrency >= 1U);
    AVAR_ASSERT(cfg.chunk_size > 0U);
}

AVAR_TEST(download_config_strategy_and_sizes) {
    setup_config();

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_ENABLED, "false"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_STRATEGY, AVAR_SEGMENT_STRATEGY_LEFT_HEAVY),
                   0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_CONCURRENCY, "8"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_CHUNK_SIZE, "32768"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_MIN_FILE_SIZE, "2048"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_LEFT_HEAVY_RATIO, "0.75"), 0);

    const DownloadSegmentConfig cfg = download_config_load(NULL);
    AVAR_ASSERT(!cfg.enabled);
    AVAR_ASSERT_EQ(cfg.strategy, SegmentStrategyLeftHeavy);
    AVAR_ASSERT_EQ(cfg.concurrency, 8U);
    AVAR_ASSERT_EQ(cfg.chunk_size, 32768U);
    AVAR_ASSERT_EQ(cfg.min_file_size, 2048U);
    AVAR_ASSERT(cfg.left_heavy_front_ratio > 0.7);
}

AVAR_TEST(download_config_queue_connection_clamp) {
    setup_config();

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_CONCURRENCY, "16"), 0);
    AVAR_ASSERT_EQ(append_config_array_item(AVAR_CFG_DM_QUEUES,
                                            "{\"" AVAR_FIELD_ID "\":\"q1\",\""
                                            AVAR_QUEUE_FIELD_MAX_CONNECTIONS "\":\"3\"}"),
                   0);

    const DownloadSegmentConfig cfg = download_config_load("q1");
    AVAR_ASSERT_EQ(cfg.concurrency, 3U);
}

AVAR_TEST(download_config_invalid_values_fall_back) {
    setup_config();

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_CONCURRENCY, "not-a-number"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_SEGMENTATION_CHUNK_SIZE, "0"), 0);

    const DownloadSegmentConfig cfg = download_config_load(NULL);
    AVAR_ASSERT_EQ(cfg.concurrency, DL_DEFAULT_SEGMENT_CONCURRENCY);
    AVAR_ASSERT_EQ(cfg.chunk_size, DL_CHUNK_SIZE);
}

AVAR_TEST_MAIN(
        run_download_config_defaults();
        run_download_config_strategy_and_sizes();
        run_download_config_queue_connection_clamp();
        run_download_config_invalid_values_fall_back();)
