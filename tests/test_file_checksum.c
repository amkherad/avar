#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_checksum.h"

static TestGuard g_guard;

AVAR_TEST(file_checksum_md5_and_compare) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-checksum"));
    char path[512];
    snprintf(path, sizeof path, "%s%ctest.txt", g_guard.work_dir, PATH_SEPARATOR);

    FILE *fp = fopen(path, "wb");
    AVAR_ASSERT_NOT_NULL(fp);
    AVAR_ASSERT_EQ(fwrite("hello", 1U, 5U, fp), 5U);
    fclose(fp);

    char *digest = NULL;
    AVAR_ASSERT_EQ(file_checksum_file(path, "md5", &digest), EXIT_SUCCESS);
    AVAR_ASSERT_NOT_NULL(digest);
    AVAR_ASSERT(strcmp(digest, "5d41402abc4b2a76b9719d911017c592") == 0);
    AVAR_ASSERT(file_checksum_matches(digest, "MD5: 5d41-402a bc4b2a76b9719d911017c592"));
    free(digest);

    AVAR_ASSERT_EQ(file_checksum_file(path, "crc32", &digest), EXIT_SUCCESS);
    AVAR_ASSERT_NOT_NULL(digest);
    AVAR_ASSERT(strcmp(digest, "3610a686") == 0);
    free(digest);
}

AVAR_TEST(file_checksum_unsupported_algorithm) {
    char *digest = NULL;
    AVAR_ASSERT(!file_checksum_algorithm_supported("sha3-256"));
    AVAR_ASSERT_EQ(file_checksum_file("missing.bin", "sha3-256", &digest), EXIT_FAILURE);
    AVAR_ASSERT_NULL(digest);
}

AVAR_TEST_MAIN(
        run_file_checksum_md5_and_compare();
        run_file_checksum_unsupported_algorithm();)
