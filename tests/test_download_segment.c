#include "avar_test.h"

#include "avar.h"
#include "download_segment.h"
#include "download_state.h"

AVAR_TEST(segment_select_left_heavy_orders_from_start) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 1024U * 1024U, 256U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    size_t indices[4] = {0};
    const size_t selected = segment_select_left_heavy(state, 3, NULL, NULL, indices);
    AVAR_ASSERT_EQ(selected, 3U);
    AVAR_ASSERT_EQ(indices[0], 0U);
    AVAR_ASSERT_EQ(indices[1], 1U);
    AVAR_ASSERT_EQ(indices[2], 2U);

    download_state_free(state);
}

AVAR_TEST(segment_select_balanced_spreads_across_file) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 8U * 256U * 1024U, 256U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    DownloadSegmentConfig cfg = {
            .enabled = true,
            .strategy = SegmentStrategyBalanced,
            .concurrency = 4U,
            .chunk_size = 256U * 1024U,
            .min_file_size = 1024U,
    };

    size_t indices[4] = {0};
    const size_t selected = segment_select_chunks(&cfg, state, 4, NULL, NULL, indices);
    AVAR_ASSERT_EQ(selected, 4U);
    AVAR_ASSERT_EQ(indices[0], 0U);
    AVAR_ASSERT_EQ(indices[1], 1U);
    AVAR_ASSERT_EQ(indices[2], 2U);
    AVAR_ASSERT_EQ(indices[3], 3U);

    download_state_free(state);
}

AVAR_TEST(segment_should_enable_respects_ranges_and_size) {
    DownloadSegmentConfig cfg = {
            .enabled = true,
            .strategy = SegmentStrategyBalanced,
            .concurrency = 4U,
            .chunk_size = DL_CHUNK_SIZE,
            .min_file_size = DL_DEFAULT_MIN_SEGMENT_FILE_SIZE,
    };

    AVAR_ASSERT(segment_should_enable(&cfg, 2U * AVAR_MIB, true));
    AVAR_ASSERT(!segment_should_enable(&cfg, 2U * AVAR_MIB, false));
    AVAR_ASSERT(!segment_should_enable(&cfg, 1024U, true));

    cfg.concurrency = 1U;
    AVAR_ASSERT(!segment_should_enable(&cfg, 2U * AVAR_MIB, true));
}

AVAR_TEST(download_state_init_chunks_preserves_done_flags) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 600000U, DL_CHUNK_SIZE);
    AVAR_ASSERT_NOT_NULL(state);
    state->chunks_done[0] = true;

    AVAR_ASSERT_EQ(download_state_init_chunks(state, 600000U, DL_CHUNK_SIZE), 0);
    AVAR_ASSERT(state->chunks_done[0]);
    AVAR_ASSERT(!state->chunks_done[1]);

    download_state_free(state);
}

AVAR_TEST(segment_chunk_range_single_byte_file) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a", 1U,
                                                 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    uint64_t start = 0U;
    uint64_t end = 0U;
    segment_chunk_range(state, 0U, &start, &end);
    AVAR_ASSERT_EQ(start, 0U);
    AVAR_ASSERT_EQ(end, 0U);

    download_state_free(state);
}

AVAR_TEST(segment_should_enable_requires_enabled_flag) {
    DownloadSegmentConfig cfg = {
            .enabled = false,
            .strategy = SegmentStrategyBalanced,
            .concurrency = 4U,
            .chunk_size = DL_CHUNK_SIZE,
            .min_file_size = 1024U,
    };

    AVAR_ASSERT(!segment_should_enable(&cfg, 2U * AVAR_MIB, true));
}

AVAR_TEST(segment_chunk_range_last_chunk_is_short) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 600000U, DL_CHUNK_SIZE);
    AVAR_ASSERT_NOT_NULL(state);

    uint64_t start = 0U;
    uint64_t end = 0U;
    segment_chunk_range(state, state->chunk_count - 1U, &start, &end);
    AVAR_ASSERT_EQ(start, 524288U);
    AVAR_ASSERT_EQ(end, 599999U);

    download_state_free(state);
}

static bool skip_chunk_one(const void *ctx, const size_t chunk_index) {
    (void)ctx;
    return chunk_index != 1U;
}

AVAR_TEST(segment_select_balanced_skips_in_flight_chunks) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 8U * 256U * 1024U, 256U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    DownloadSegmentConfig cfg = {
            .enabled = true,
            .strategy = SegmentStrategyBalanced,
            .concurrency = 4U,
            .chunk_size = 256U * 1024U,
            .min_file_size = 1024U,
    };

    size_t indices[4] = {0};
    const size_t selected = segment_select_chunks(&cfg, state, 4, skip_chunk_one, NULL, indices);
    AVAR_ASSERT_EQ(selected, 4U);
    for (size_t i = 0; i < selected; i++) {
        AVAR_ASSERT(indices[i] != 1U);
    }

    download_state_free(state);
}

AVAR_TEST_MAIN(
        run_segment_select_left_heavy_orders_from_start();
        run_segment_select_balanced_spreads_across_file();
        run_segment_should_enable_respects_ranges_and_size();
        run_download_state_init_chunks_preserves_done_flags();
        run_segment_chunk_range_single_byte_file();
        run_segment_should_enable_requires_enabled_flag();
        run_segment_chunk_range_last_chunk_is_short();
        run_segment_select_balanced_skips_in_flight_chunks();)
