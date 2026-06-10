#include "avar_test.h"

#include <stdio.h>

#include "avar.h"
#include "utils.h"

static stringa http_schemes[] = {"http", "https", NULL};

static char *capture_print_help(int message_n, const char *const *messages) {
    char tmp_path[L_tmpnam];
    if (tmpnam(tmp_path) == NULL) {
        return NULL;
    }

    if (freopen(tmp_path, "w+", stdout) == NULL) {
        return NULL;
    }

    (void) print_help(message_n, (const char **) messages);
    fflush(stdout);

#if defined(_WIN32)
    freopen("CONOUT$", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif

    FILE *tmp = fopen(tmp_path, "rb");
    if (tmp == NULL) {
        return NULL;
    }

    char *output = NULL;
    size_t cap = 0;
    size_t len = 0;
    char buffer[256];

    while (fgets(buffer, sizeof buffer, tmp) != NULL) {
        const size_t chunk = strlen(buffer);
        if (len + chunk + 1 > cap) {
            cap = cap == 0 ? 256 : cap * 2;
            char *next = realloc(output, cap);
            if (next == NULL) {
                free(output);
                fclose(tmp);
                remove(tmp_path);
                return NULL;
            }
            output = next;
        }
        memcpy(output + len, buffer, chunk);
        len += chunk;
        output[len] = '\0';
    }

    fclose(tmp);
    remove(tmp_path);
    return output;
}

AVAR_TEST(utils_is_valid_url_rejects_null) {
    AVAR_ASSERT(!is_valid_url(NULL, http_schemes));
}

AVAR_TEST(utils_is_valid_url_rejects_missing_scheme) {
    AVAR_ASSERT(!is_valid_url("example.com", http_schemes));
    AVAR_ASSERT(!is_valid_url("example.com/path", http_schemes));
}

AVAR_TEST(utils_is_valid_url_rejects_unsupported_scheme) {
    AVAR_ASSERT(!is_valid_url("ftp://example.com", http_schemes));
    AVAR_ASSERT(!is_valid_url("file:///etc/passwd", http_schemes));
}

AVAR_TEST(utils_is_valid_url_rejects_empty_authority) {
    AVAR_ASSERT(!is_valid_url("http://", http_schemes));
    AVAR_ASSERT(!is_valid_url("https://", http_schemes));
}

AVAR_TEST(utils_is_valid_url_rejects_invalid_host) {
    AVAR_ASSERT(!is_valid_url("http://", http_schemes));
    AVAR_ASSERT(!is_valid_url("http:// exam.com", http_schemes));
    AVAR_ASSERT(!is_valid_url("http://exam ple.com", http_schemes));
    AVAR_ASSERT(!is_valid_url("http://exam\tple.com", http_schemes));
}

AVAR_TEST(utils_is_valid_url_accepts_minimal_http_urls) {
    AVAR_ASSERT(is_valid_url("http://example.com", http_schemes));
    AVAR_ASSERT(is_valid_url("https://example.com", http_schemes));
}

AVAR_TEST(utils_is_valid_url_accepts_paths_queries_and_fragments) {
    AVAR_ASSERT(is_valid_url("http://example.com/path/to/resource", http_schemes));
    AVAR_ASSERT(is_valid_url("http://example.com?query=value", http_schemes));
    AVAR_ASSERT(is_valid_url("http://example.com#fragment", http_schemes));
    AVAR_ASSERT(is_valid_url("https://example.com/a/b?c=d#e", http_schemes));
}

AVAR_TEST(utils_is_valid_url_accepts_userinfo_and_port) {
    AVAR_ASSERT(is_valid_url("http://user:pass@example.com", http_schemes));
    AVAR_ASSERT(is_valid_url("http://example.com:8080", http_schemes));
    AVAR_ASSERT(is_valid_url("http://user@example.com:8080/path", http_schemes));
}

AVAR_TEST(utils_is_valid_http_url_delegates_to_http_schemes) {
    AVAR_ASSERT(is_valid_http_url("http://example.com"));
    AVAR_ASSERT(is_valid_http_url("https://example.com/path"));
    AVAR_ASSERT(!is_valid_http_url("ftp://example.com"));
    AVAR_ASSERT(!is_valid_http_url(NULL));
}

AVAR_TEST(utils_print_help_returns_success) {
    const char *messages[] = {
            "Avar",
            "Usage: avar [options]",
            "  --help  Show help",
    };

    AVAR_ASSERT_EQ(print_help(3, messages), EXIT_SUCCESS);
#if !defined(_WIN32)
    char *output = capture_print_help(3, messages);
    AVAR_ASSERT_NOT_NULL(output);
    AVAR_ASSERT_STR_EQ(output,
                       "Avar (v" VERSION_STR ")\n"
                       "Usage: avar [options]\n"
                       "  --help  Show help\n");
    free(output);
#endif
}

AVAR_TEST(utils_format_progress_bar_renders_percent_fill) {
    char bar[32];
    format_progress_bar(50, 22, bar, sizeof bar);
    AVAR_ASSERT_STR_EQ(bar, "[===========           ]");
}

AVAR_TEST(utils_format_data_size_uses_configured_units) {
    char buf[32];

    format_data_size(10U * AVAR_MIB, AVAR_SIZE_MIB, buf, sizeof buf);
    AVAR_ASSERT_STR_EQ(buf, "10 " AVAR_UNIT_MIB);

    format_data_size(100U * AVAR_MIB, AVAR_SIZE_MIB, buf, sizeof buf);
    AVAR_ASSERT_STR_EQ(buf, "100 " AVAR_UNIT_MIB);
}

AVAR_TEST(utils_format_transfer_rate_uses_configured_units) {
    char buf[32];

    format_transfer_rate((double) (5U * AVAR_MIB) / (double) AVAR_BITS_PER_BYTE,
                         AVAR_SPEED_MIBIT_PER_SEC, buf, sizeof buf);
    AVAR_ASSERT_STR_EQ(buf, "5 " AVAR_UNIT_MIBIT_PER_SEC);
}

AVAR_TEST(utils_unit_parsers_accept_config_keys) {
    AvarSizeUnit size_unit = AVAR_SIZE_BYTES;
    AvarSpeedUnit speed_unit = AVAR_SPEED_BYTES_PER_SEC;

    AVAR_ASSERT(avar_size_unit_parse(AVAR_UNIT_MIB, &size_unit));
    AVAR_ASSERT_EQ(size_unit, AVAR_SIZE_MIB);

    AVAR_ASSERT(avar_speed_unit_parse(AVAR_UNIT_MIBIT_PER_SEC, &speed_unit));
    AVAR_ASSERT_EQ(speed_unit, AVAR_SPEED_MIBIT_PER_SEC);
}

AVAR_TEST(utils_size_unit_rejects_kibibits) {
    AvarSizeUnit size_unit = AVAR_SIZE_MIB;

    AVAR_ASSERT(!avar_size_unit_parse("Kib", &size_unit));
    AVAR_ASSERT_EQ(size_unit, AVAR_SIZE_MIB);
}

AVAR_TEST(utils_print_help_handles_single_line) {
    const char *messages[] = {"Avar"};

    AVAR_ASSERT_EQ(print_help(1, messages), EXIT_SUCCESS);
#if !defined(_WIN32)
    char *output = capture_print_help(1, messages);
    AVAR_ASSERT_NOT_NULL(output);
    AVAR_ASSERT_STR_EQ(output, "Avar (v" VERSION_STR ")\n");
    free(output);
#endif
}

AVAR_TEST_MAIN(
        run_utils_is_valid_url_rejects_null();
        run_utils_is_valid_url_rejects_missing_scheme();
        run_utils_is_valid_url_rejects_unsupported_scheme();
        run_utils_is_valid_url_rejects_empty_authority();
        run_utils_is_valid_url_rejects_invalid_host();
        run_utils_is_valid_url_accepts_minimal_http_urls();
        run_utils_is_valid_url_accepts_paths_queries_and_fragments();
        run_utils_is_valid_url_accepts_userinfo_and_port();
        run_utils_is_valid_http_url_delegates_to_http_schemes();
        run_utils_print_help_returns_success();
        run_utils_format_progress_bar_renders_percent_fill();
        run_utils_format_data_size_uses_configured_units();
        run_utils_format_transfer_rate_uses_configured_units();
        run_utils_unit_parsers_accept_config_keys();
        run_utils_size_unit_rejects_kibibits();
        run_utils_print_help_handles_single_line();)
