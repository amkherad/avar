#include "avar_test.h"
#include "test_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "download.h"
#include "config.h"
#include "file-system.h"

static TestGuard g_guard;

AVAR_TEST(download_internals_choose_filename) {
    char *from_url = download_test_choose_filename("http://example.com/path/file.bin", NULL, 0U);
    AVAR_ASSERT_NOT_NULL(from_url);
    AVAR_ASSERT_STR_EQ(from_url, "file.bin");
    free(from_url);

    const char *cd = "attachment; filename=\"report.pdf\"";
    char *from_header = download_test_choose_filename("http://example.com/x", cd, strlen(cd));
    AVAR_ASSERT_NOT_NULL(from_header);
    AVAR_ASSERT_STR_EQ(from_header, "report.pdf");
    free(from_header);
}

AVAR_TEST(download_internals_parse_content_range) {
    uint64_t total = 0U;
    AVAR_ASSERT(download_test_parse_content_range_total("bytes 0-99/1000", &total));
    AVAR_ASSERT_EQ(total, 1000U);

    AVAR_ASSERT(!download_test_parse_content_range_total("invalid", &total));
    AVAR_ASSERT(!download_test_parse_content_range_total(NULL, &total));
}

AVAR_TEST(download_internals_existing_file_size) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-dl-internals"));
    (void)make_dirs_in_path(g_guard.work_dir);

    char path[512];
    snprintf(path, sizeof path, "%s%csample.txt", g_guard.work_dir, PATH_SEPARATOR);
    FILE *file = fopen(path, "wb");
    AVAR_ASSERT_NOT_NULL(file);
    AVAR_ASSERT(fwrite("hello", 1U, 5U, file) == 5U);
    fclose(file);

    AVAR_ASSERT_EQ(download_test_existing_file_size(path), 5U);
    AVAR_ASSERT_EQ(download_test_existing_file_size("missing-file-xyz"), 0U);
    remove(path);
}

AVAR_TEST(download_internals_generate_id) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-dl-internals-id"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);

    char *id1 = download_test_generate_id();
    char *id2 = download_test_generate_id();
    AVAR_ASSERT_NOT_NULL(id1);
    AVAR_ASSERT_NOT_NULL(id2);
    AVAR_ASSERT(strncmp(id1, AVAR_DL_ID_PREFIX, strlen(AVAR_DL_ID_PREFIX)) == 0);
    AVAR_ASSERT(strcmp(id1, id2) != 0);
    free(id1);
    free(id2);
}

AVAR_TEST_MAIN(
        run_download_internals_choose_filename();
        run_download_internals_parse_content_range();
        run_download_internals_existing_file_size();
        run_download_internals_generate_id();)
