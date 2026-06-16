#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>

#include "avar.h"
#include "download_state.h"
#include "file-system.h"

static TestGuard g_guard;

static void setup_temp_state_path(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-state"));
    snprintf(g_guard.stats_path, sizeof g_guard.stats_path, "%s%cstate.json", g_guard.work_dir,
             PATH_SEPARATOR);
    remove(g_guard.stats_path);
}

AVAR_TEST(download_state_save_load_roundtrip) {
    setup_temp_state_path();

    DownloadState *state = download_state_create("https://example.com/a.bin", "a.bin", "/tmp/a.bin",
                                                 "/dl/a.bin", 600000, DL_CHUNK_SIZE);
    AVAR_ASSERT_NOT_NULL(state);

    AVAR_ASSERT_EQ(download_state_mark_range_done(state, 0U, DL_CHUNK_SIZE - 1U), 0);

    const uint64_t seg2_start = 2ULL * DL_CHUNK_SIZE;
    const uint64_t seg2_end = 599999ULL;
    AVAR_ASSERT_EQ(download_state_mark_range_done(state, seg2_start, seg2_end), 0);

    AVAR_ASSERT_EQ(download_state_save(state, g_guard.stats_path), 0);

    DownloadState *loaded = download_state_load(g_guard.stats_path);
    AVAR_ASSERT_NOT_NULL(loaded);
    AVAR_ASSERT_STR_EQ(loaded->url, "https://example.com/a.bin");
    AVAR_ASSERT_EQ(loaded->total_size, 600000ULL);
    AVAR_ASSERT(download_state_is_segment_done(loaded, 0U));
    AVAR_ASSERT(!download_state_is_segment_done(loaded, 1U));
    AVAR_ASSERT(download_state_is_segment_done(loaded, 2U));
    AVAR_ASSERT_EQ(download_state_bytes_done(loaded),
                  (uint64_t)(DL_CHUNK_SIZE + (600000 - 2 * DL_CHUNK_SIZE)));

    download_state_free(state);
    download_state_free(loaded);
    remove(g_guard.stats_path);
}

AVAR_TEST(download_state_bytes_done_empty) {
    DownloadState *state = download_state_create("https://x", "x", "/t", "/d", 0, DL_CHUNK_SIZE);
    AVAR_ASSERT_NOT_NULL(state);
    AVAR_ASSERT_EQ(download_state_bytes_done(state), 0ULL);
    download_state_free(state);
}

AVAR_TEST(download_state_merges_adjacent_ranges) {
    DownloadState *state = download_state_create("https://x", "x", "/t", "/d", 600000, DL_CHUNK_SIZE);
    AVAR_ASSERT_NOT_NULL(state);

    AVAR_ASSERT_EQ(download_state_mark_range_done(state, 0U, DL_CHUNK_SIZE - 1U), 0);
    AVAR_ASSERT_EQ(download_state_mark_range_done(state, DL_CHUNK_SIZE, 2U * DL_CHUNK_SIZE - 1U), 0);
    AVAR_ASSERT_EQ(state->done_range_count, 1U);
    AVAR_ASSERT_EQ(state->done_ranges[0].start, 0U);
    AVAR_ASSERT_EQ(state->done_ranges[0].end, 2U * DL_CHUNK_SIZE - 1U);

    download_state_free(state);
}

AVAR_TEST_MAIN(
        run_download_state_save_load_roundtrip();
        run_download_state_bytes_done_empty();
        run_download_state_merges_adjacent_ranges();)
