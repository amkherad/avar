#include "avar_test.h"

#include "avar.h"
#include "download_segment.h"
#include "download_state.h"

AVAR_TEST(segment_select_left_heavy_orders_from_start) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 1024U * 1024U, 256U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    ByteRange ranges[4] = {{0}};
    const size_t selected = segment_select_left_heavy(state, 3, NULL, NULL, ranges);
    AVAR_ASSERT_EQ(selected, 3U);
    AVAR_ASSERT_EQ(ranges[0].start, 0U);
    AVAR_ASSERT_EQ(ranges[1].start, 256U * 1024U);
    AVAR_ASSERT_EQ(ranges[2].start, 512U * 1024U);

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

    ByteRange ranges[4] = {{0}};
    const size_t selected = segment_select_chunks(&cfg, state, 4, NULL, NULL, ranges);
    AVAR_ASSERT_EQ(selected, 4U);
    AVAR_ASSERT_EQ(ranges[0].start, 0U);
    AVAR_ASSERT_EQ(ranges[1].start, 2U * 256U * 1024U);
    AVAR_ASSERT_EQ(ranges[2].start, 4U * 256U * 1024U);
    AVAR_ASSERT_EQ(ranges[3].start, 6U * 256U * 1024U);

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

AVAR_TEST(download_state_init_chunks_preserves_done_ranges) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 600000U, DL_CHUNK_SIZE);
    AVAR_ASSERT_NOT_NULL(state);
    AVAR_ASSERT_EQ(download_state_mark_range_done(state, 0U, DL_CHUNK_SIZE - 1U), 0);

    AVAR_ASSERT_EQ(download_state_init_chunks(state, 600000U, DL_CHUNK_SIZE), 0);
    AVAR_ASSERT(download_state_is_segment_done(state, 0U));
    AVAR_ASSERT(!download_state_is_segment_done(state, 1U));

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
    const size_t last_segment = download_state_segment_count(state) - 1U;
    segment_chunk_range(state, last_segment, &start, &end);
    AVAR_ASSERT_EQ(start, 524288U);
    AVAR_ASSERT_EQ(end, 599999U);

    download_state_free(state);
}

static bool skip_segment_one(const void *ctx, const uint64_t start, const uint64_t end) {
    (void)ctx;
    uint64_t seg_start = 0U;
    uint64_t seg_end = 0U;
    segment_index_range(8U * 256U * 1024U, 256U * 1024U, 1U, &seg_start, &seg_end);
    return start != seg_start || end != seg_end;
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

    ByteRange ranges[4] = {{0}};
    const size_t selected = segment_select_chunks(&cfg, state, 4, skip_segment_one, NULL, ranges);
    AVAR_ASSERT_EQ(selected, 4U);

    uint64_t seg_start = 0U;
    uint64_t seg_end = 0U;
    segment_index_range(8U * 256U * 1024U, 256U * 1024U, 1U, &seg_start, &seg_end);
    for (size_t i = 0; i < selected; i++) {
        AVAR_ASSERT(ranges[i].start != seg_start || ranges[i].end != seg_end);
    }

    download_state_free(state);
}

/* Already-completed segments must never be re-selected. */
AVAR_TEST(segment_select_skips_completed_segments) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 4U * 256U * 1024U, 256U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);
    AVAR_ASSERT_EQ(download_state_mark_range_done(state, 0U, 256U * 1024U - 1U), 0);

    DownloadSegmentConfig cfg = {
            .enabled = true,
            .strategy = SegmentStrategyBalanced,
            .concurrency = 4U,
            .chunk_size = 256U * 1024U,
            .min_file_size = 1024U,
    };

    ByteRange ranges[4] = {{0}};
    const size_t selected = segment_select_chunks(&cfg, state, 4, NULL, NULL, ranges);
    AVAR_ASSERT_EQ(selected, 3U);
    for (size_t i = 0; i < selected; i++) {
        AVAR_ASSERT(ranges[i].start != 0U);
    }

    download_state_free(state);
}

/* When concurrency exceeds the number of segments, only the real segments are
 * returned and there are no duplicates. */
AVAR_TEST(segment_select_balanced_concurrency_exceeds_segments) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 2U * 256U * 1024U, 256U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    ByteRange ranges[8] = {{0}};
    const size_t selected = segment_select_balanced(state, 8U, 8U, NULL, NULL, ranges);
    AVAR_ASSERT_EQ(selected, 2U);
    AVAR_ASSERT(ranges[0].start != ranges[1].start);

    download_state_free(state);
}

/* The selector must honour max_count even when more segments are pending. */
AVAR_TEST(segment_select_respects_max_count) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 8U * 256U * 1024U, 256U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    DownloadSegmentConfig cfg = {
            .enabled = true,
            .strategy = SegmentStrategyBalanced,
            .concurrency = 8U,
            .chunk_size = 256U * 1024U,
            .min_file_size = 1024U,
    };

    ByteRange ranges[2] = {{0}};
    const size_t selected = segment_select_chunks(&cfg, state, 2, NULL, NULL, ranges);
    AVAR_ASSERT_EQ(selected, 2U);

    download_state_free(state);
}

AVAR_TEST(segment_ranges_overlap_detects_shared_bytes) {
    AVAR_ASSERT(segment_ranges_overlap(0U, 100U, 50U, 150U));
    AVAR_ASSERT(segment_ranges_overlap(50U, 150U, 0U, 100U));
    AVAR_ASSERT(!segment_ranges_overlap(0U, 99U, 100U, 199U));
    AVAR_ASSERT(segment_ranges_overlap(100U, 100U, 100U, 100U));
}

AVAR_TEST(segment_next_reserve_end_extends_when_clear) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 512U * 1024U, 64U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    const uint64_t extended =
            segment_next_reserve_end(state, NULL, 0U, 64U * 1024U - 1U);
    AVAR_ASSERT_EQ(extended, 128U * 1024U - 1U);

    download_state_free(state);
}

AVAR_TEST(segment_next_reserve_end_blocks_on_in_flight) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 512U * 1024U, 64U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);

    const ByteRange in_flight[] = {{.start = 128U * 1024U, .end = 192U * 1024U - 1U}};
    const uint64_t extended =
            segment_next_reserve_end(state, in_flight, 1U, 128U * 1024U - 1U);
    AVAR_ASSERT_EQ(extended, 128U * 1024U - 1U);

    download_state_free(state);
}

AVAR_TEST(segment_next_reserve_end_blocks_on_done_range) {
    DownloadState *state = download_state_create("https://example.com/a", "a.bin", "/t/a", "/d/a",
                                                 512U * 1024U, 64U * 1024U);
    AVAR_ASSERT_NOT_NULL(state);
    AVAR_ASSERT_EQ(download_state_mark_range_done(state, 64U * 1024U, 128U * 1024U - 1U), 0);

    const uint64_t extended =
            segment_next_reserve_end(state, NULL, 0U, 64U * 1024U - 1U);
    AVAR_ASSERT_EQ(extended, 64U * 1024U - 1U);

    download_state_free(state);
}

AVAR_TEST_MAIN(
        run_segment_select_left_heavy_orders_from_start();
        run_segment_select_balanced_spreads_across_file();
        run_segment_should_enable_respects_ranges_and_size();
        run_download_state_init_chunks_preserves_done_ranges();
        run_segment_chunk_range_single_byte_file();
        run_segment_should_enable_requires_enabled_flag();
        run_segment_chunk_range_last_chunk_is_short();
        run_segment_select_balanced_skips_in_flight_chunks();
        run_segment_select_skips_completed_segments();
        run_segment_select_balanced_concurrency_exceeds_segments();
        run_segment_select_respects_max_count();
        run_segment_ranges_overlap_detects_shared_bytes();
        run_segment_next_reserve_end_extends_when_clear();
        run_segment_next_reserve_end_blocks_on_in_flight();
        run_segment_next_reserve_end_blocks_on_done_range();)
