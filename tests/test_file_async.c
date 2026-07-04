#include "avar_test.h"
#include "test_guard.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_async.h"

static TestGuard g_guard;
static char g_path[600];

static void setup_path(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-async"));
    snprintf(g_path, sizeof g_path, "%s%casync.bin", g_guard.work_dir,
#if defined(_WIN32)
             '\\');
#else
             '/');
#endif
    remove(g_path);
}

/* Reads the whole file at g_path into buf (up to cap); returns byte count. */
static size_t read_file(unsigned char *buf, size_t cap) {
    FILE *fp = fopen(g_path, "rb");
    if (fp == NULL) {
        return 0;
    }
    const size_t n = fread(buf, 1, cap, fp);
    fclose(fp);
    return n;
}

AVAR_TEST(file_async_contiguous_write_round_trips) {
    setup_path();

    AvarAsyncFile *file = avar_async_file_open(g_path, "wb");
    AVAR_ASSERT_NOT_NULL(file);

    const char *msg = "hello, async world";
    const size_t len = strlen(msg);
    AVAR_ASSERT_EQ(avar_async_file_write(file, 0U, msg, len), 0);
    AVAR_ASSERT_EQ(avar_async_file_close(file), 0);

    unsigned char buf[64] = {0};
    AVAR_ASSERT_EQ(read_file(buf, sizeof buf), len);
    AVAR_ASSERT(memcmp(buf, msg, len) == 0);
}

AVAR_TEST(file_async_drain_flushes_to_disk) {
    setup_path();

    AvarAsyncFile *file = avar_async_file_open(g_path, "wb");
    AVAR_ASSERT_NOT_NULL(file);

    const char *msg = "durable";
    AVAR_ASSERT_EQ(avar_async_file_write(file, 0U, msg, strlen(msg)), 0);
    /* After drain the OS must be able to see the bytes without a close. */
    AVAR_ASSERT_EQ(avar_async_file_drain(file), 0);

    unsigned char buf[16] = {0};
    AVAR_ASSERT_EQ(read_file(buf, sizeof buf), strlen(msg));
    AVAR_ASSERT(memcmp(buf, msg, strlen(msg)) == 0);

    AVAR_ASSERT_EQ(avar_async_file_close(file), 0);
}

AVAR_TEST(file_async_non_contiguous_writes_land_at_offsets) {
    setup_path();

    AvarAsyncFile *file = avar_async_file_open(g_path, "wb");
    AVAR_ASSERT_NOT_NULL(file);

    /* Write the tail first, then the head: forces a buffer swap on the
     * non-contiguous jump and exercises seeking. */
    AVAR_ASSERT_EQ(avar_async_file_write(file, 8U, "TAIL", 4U), 0);
    AVAR_ASSERT_EQ(avar_async_file_write(file, 0U, "HEAD", 4U), 0);
    AVAR_ASSERT_EQ(avar_async_file_close(file), 0);

    unsigned char buf[16] = {0};
    AVAR_ASSERT_EQ(read_file(buf, sizeof buf), 12U);
    AVAR_ASSERT(memcmp(buf, "HEAD", 4U) == 0);
    AVAR_ASSERT(memcmp(buf + 8U, "TAIL", 4U) == 0);
}

AVAR_TEST(file_async_large_stream_exceeds_buffers) {
    setup_path();

    AvarAsyncFile *file = avar_async_file_open(g_path, "wb");
    AVAR_ASSERT_NOT_NULL(file);

    /* Several buffers' worth, written in odd-sized contiguous chunks so the
     * producer runs ahead of the worker and hits backpressure. */
    enum { TOTAL = 3U * 1024U * 1024U, STEP = 40000U };
    unsigned char *src = malloc(TOTAL);
    AVAR_ASSERT_NOT_NULL(src);
    for (size_t i = 0; i < TOTAL; i++) {
        src[i] = (unsigned char)(i * 31U + 7U);
    }

    uint64_t offset = 0U;
    while (offset < TOTAL) {
        const size_t chunk = (TOTAL - offset) < STEP ? (size_t)(TOTAL - offset) : STEP;
        AVAR_ASSERT_EQ(avar_async_file_write(file, offset, src + offset, chunk), 0);
        offset += chunk;
    }
    AVAR_ASSERT_EQ(avar_async_file_error(file), 0);
    AVAR_ASSERT_EQ(avar_async_file_close(file), 0);

    unsigned char *dst = malloc(TOTAL + 16U);
    AVAR_ASSERT_NOT_NULL(dst);
    AVAR_ASSERT_EQ(read_file(dst, TOTAL + 16U), (size_t)TOTAL);
    AVAR_ASSERT(memcmp(dst, src, TOTAL) == 0);

    free(src);
    free(dst);
}

AVAR_TEST(file_async_open_rejects_bad_path) {
    AVAR_ASSERT_NULL(avar_async_file_open(NULL, "wb"));
    AVAR_ASSERT_NULL(avar_async_file_open("some/path", NULL));
    /* Closing NULL is a no-op success. */
    AVAR_ASSERT_EQ(avar_async_file_close(NULL), 0);
}

AVAR_TEST_MAIN(
        run_file_async_contiguous_write_round_trips();
        run_file_async_drain_flushes_to_disk();
        run_file_async_non_contiguous_writes_land_at_offsets();
        run_file_async_large_stream_exceeds_buffers();
        run_file_async_open_rejects_bad_path();)
