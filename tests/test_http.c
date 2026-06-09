#include "avar_test.h"

#include "http.h"

AVAR_TEST(http_url_basename_decodes_path) {
    char *name = http_url_basename("https://example.com/files/hello%20world.txt");
    AVAR_ASSERT_NOT_NULL(name);
    AVAR_ASSERT_STR_EQ(name, "hello world.txt");
    free(name);
}

AVAR_TEST(http_content_disposition_filename) {
    const char *header = "attachment; filename=\"report.csv\"";
    char *name = http_parse_content_disposition_filename(header, strlen(header));
    AVAR_ASSERT_NOT_NULL(name);
    AVAR_ASSERT_STR_EQ(name, "report.csv");
    free(name);
}

AVAR_TEST(http_resolve_relative_redirect) {
    char *resolved = http_resolve_url("https://example.com/dir/page", "other/file.bin", 14);
    AVAR_ASSERT_NOT_NULL(resolved);
    AVAR_ASSERT_STR_EQ(resolved, "https://example.com/dir/other/file.bin");
    free(resolved);
}

AVAR_TEST(http_resolve_protocol_relative_redirect) {
    const char *location = "//cdn.example.com/files/app.zip";
    char *resolved = http_resolve_url("https://example.com/dir/page", location, strlen(location));
    AVAR_ASSERT_NOT_NULL(resolved);
    AVAR_ASSERT_STR_EQ(resolved, "https://cdn.example.com/files/app.zip");
    free(resolved);
}

AVAR_TEST(http_resolve_redirect_trims_whitespace) {
    char *resolved = http_resolve_url("https://example.com/a", " /b.bin \r\n", 9);
    AVAR_ASSERT_NOT_NULL(resolved);
    AVAR_ASSERT_STR_EQ(resolved, "https://example.com/b.bin");
    free(resolved);
}

AVAR_TEST(http_redirect_detection) {
    AVAR_ASSERT(http_is_redirect(302));
    AVAR_ASSERT(!http_is_redirect(200));
}

AVAR_TEST_MAIN(
        run_http_url_basename_decodes_path();
        run_http_content_disposition_filename();
        run_http_resolve_relative_redirect();
        run_http_resolve_protocol_relative_redirect();
        run_http_resolve_redirect_trims_whitespace();
        run_http_redirect_detection();)
