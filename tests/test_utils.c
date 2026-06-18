#include "avar_test.h"

#include <stdio.h>

#if !defined(_WIN32)
    #include <fcntl.h>
    #include <unistd.h>
#endif

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

AVAR_TEST(utils_trim_whitespace_inplace_trims_ends) {
    char value[] = "  https://example.com/file.mkv  ";
    trim_whitespace_inplace(value);
    AVAR_ASSERT_STR_EQ(value, "https://example.com/file.mkv");
}

AVAR_TEST(utils_is_valid_http_url_accepts_trimmed_urls) {
    char value[] = "https://example.com/file.mkv ";
    trim_whitespace_inplace(value);
    AVAR_ASSERT(is_valid_http_url(value));
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

AVAR_TEST(utils_format_spatial_progress_bar_renders_separate_regions) {
    const AvarByteRange ranges[] = {{0, 24}, {50, 74}};
    char bar[32];

    format_spatial_progress_bar(100, ranges, 2, 20, bar, sizeof bar);
    AVAR_ASSERT_STR_EQ(bar, "[=====     =====     ]");
}

AVAR_TEST(utils_progress_style_parse_accepts_known_values) {
    AvarProgressStyle style = AVAR_PROGRESS_SEGMENTED;

    AVAR_ASSERT(avar_progress_style_parse(AVAR_PROGRESS_STYLE_AGGREGATE, &style));
    AVAR_ASSERT_EQ(style, AVAR_PROGRESS_AGGREGATE);

    AVAR_ASSERT(avar_progress_style_parse(AVAR_PROGRESS_STYLE_SEGMENTED, &style));
    AVAR_ASSERT_EQ(style, AVAR_PROGRESS_SEGMENTED);

    AVAR_ASSERT(avar_progress_style_parse(NULL, &style));
    AVAR_ASSERT_EQ(style, AVAR_PROGRESS_AGGREGATE);

    AVAR_ASSERT(!avar_progress_style_parse("invalid", &style));
}

AVAR_TEST(utils_progress_bar_width_accounts_for_brackets) {
    AVAR_ASSERT_EQ(avar_progress_bar_width_for_columns(80, 40), 38);
    AVAR_ASSERT_EQ(avar_progress_bar_width_for_columns(0, 40), DL_PROGRESS_BAR_WIDTH);
}

AVAR_TEST(utils_progress_bar_width_uses_fixed_size_without_tty) {
#if !defined(_WIN32)
    const int fd = dup(STDERR_FILENO);
    AVAR_ASSERT(fd >= 0);

    int devnull = open("/dev/null", O_WRONLY);
    AVAR_ASSERT(devnull >= 0);
    AVAR_ASSERT(dup2(devnull, STDERR_FILENO) >= 0);

    AVAR_ASSERT_EQ(avar_progress_bar_width(40), DL_PROGRESS_BAR_WIDTH);

    AVAR_ASSERT(dup2(fd, STDERR_FILENO) >= 0);
    close(fd);
    close(devnull);
#endif
}

AVAR_TEST(utils_format_progress_percent_right_aligns) {
    char buf[8];

    format_progress_percent(7, buf, sizeof buf);
    AVAR_ASSERT_STR_EQ(buf, "  7%");

    format_progress_percent(47, buf, sizeof buf);
    AVAR_ASSERT_STR_EQ(buf, " 47%");

    format_progress_percent(100, buf, sizeof buf);
    AVAR_ASSERT_STR_EQ(buf, "100%");
}

AVAR_TEST(utils_data_size_number_width_matches_value) {
    AVAR_ASSERT_EQ(avar_data_size_number_width(7U * AVAR_MIB, AVAR_SIZE_MIB), 1);
    AVAR_ASSERT_EQ(avar_data_size_number_width(10U * AVAR_MIB, AVAR_SIZE_MIB), 2);
    AVAR_ASSERT_EQ(avar_data_size_number_width(192U * AVAR_MIB, AVAR_SIZE_MIB), 3);
}

AVAR_TEST(utils_padded_size_uses_inferred_width) {
    char buf[32];
    const int width = avar_data_size_number_width(192U * AVAR_MIB, AVAR_SIZE_MIB);

    format_data_size_padded(192U * AVAR_MIB, AVAR_SIZE_MIB, width, buf, sizeof buf);
    AVAR_ASSERT_STR_EQ(buf, "192 " AVAR_UNIT_MIB);
}

AVAR_TEST(utils_padded_transfer_rate_uses_fixed_width) {
    char buf[32];

    format_transfer_rate_padded((double) (5U * AVAR_MIB) / (double) AVAR_BITS_PER_BYTE,
                                AVAR_SPEED_MIBIT_PER_SEC, DL_PROGRESS_SPEED_NUMBER_WIDTH, buf,
                                sizeof buf);
    AVAR_ASSERT_STR_EQ(buf, "     5 " AVAR_UNIT_MIBIT_PER_SEC);
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

AVAR_TEST(utils_strndup_truncates) {
    char *copy = strndup("hello world", 5U);
    AVAR_ASSERT_NOT_NULL(copy);
    AVAR_ASSERT_STR_EQ(copy, "hello");
    free(copy);

    AVAR_ASSERT_NULL(strndup(NULL, 4U));
}

AVAR_TEST(utils_stderr_is_tty_is_boolean) {
    (void)avar_stderr_is_tty();
    (void)avar_terminal_columns();
    (void)avar_progress_bar_width(10);
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
        run_utils_trim_whitespace_inplace_trims_ends();
        run_utils_is_valid_http_url_accepts_trimmed_urls();
        run_utils_print_help_returns_success();
        run_utils_format_progress_bar_renders_percent_fill();
        run_utils_format_spatial_progress_bar_renders_separate_regions();
        run_utils_progress_style_parse_accepts_known_values();
        run_utils_progress_bar_width_accounts_for_brackets();
        run_utils_progress_bar_width_uses_fixed_size_without_tty();
        run_utils_format_progress_percent_right_aligns();
        run_utils_data_size_number_width_matches_value();
        run_utils_padded_size_uses_inferred_width();
        run_utils_padded_transfer_rate_uses_fixed_width();
        run_utils_format_data_size_uses_configured_units();
        run_utils_format_transfer_rate_uses_configured_units();
        run_utils_unit_parsers_accept_config_keys();
        run_utils_size_unit_rejects_kibibits();
        run_utils_print_help_handles_single_line();
        run_utils_strndup_truncates();
        run_utils_stderr_is_tty_is_boolean();)
