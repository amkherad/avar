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

    state->chunks_done[0] = true;
    state->chunks_done[2] = true;

    AVAR_ASSERT_EQ(download_state_save(state, g_guard.stats_path), 0);

    DownloadState *loaded = download_state_load(g_guard.stats_path);
    AVAR_ASSERT_NOT_NULL(loaded);
    AVAR_ASSERT_STR_EQ(loaded->url, "https://example.com/a.bin");
    AVAR_ASSERT_EQ(loaded->total_size, 600000ULL);
    AVAR_ASSERT_EQ(loaded->chunk_count, 3U);
    AVAR_ASSERT(loaded->chunks_done[0]);
    AVAR_ASSERT(!loaded->chunks_done[1]);
    AVAR_ASSERT(loaded->chunks_done[2]);
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

AVAR_TEST_MAIN(
        run_download_state_save_load_roundtrip();
        run_download_state_bytes_done_empty();)
