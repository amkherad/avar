#include <criterion/criterion.h>
#include <criterion/redirect.h>

#include <stdlib.h>
#include <string.h>

#include "utils.h"

#define TEST_VERSION_STR "0.0.1"

static stringa http_schemes[] = {"http", "https", NULL};

static void redirect_stdout(void)
{
    cr_redirect_stdout();
}

Test(utils, is_valid_url_rejects_null)
{
    cr_assert_not(is_valid_url(NULL, http_schemes));
}

Test(utils, is_valid_url_rejects_missing_scheme)
{
    cr_assert_not(is_valid_url("example.com", http_schemes));
    cr_assert_not(is_valid_url("example.com/path", http_schemes));
}

Test(utils, is_valid_url_rejects_unsupported_scheme)
{
    cr_assert_not(is_valid_url("ftp://example.com", http_schemes));
    cr_assert_not(is_valid_url("file:///etc/passwd", http_schemes));
}

Test(utils, is_valid_url_rejects_empty_authority)
{
    cr_assert_not(is_valid_url("http://", http_schemes));
    cr_assert_not(is_valid_url("https://", http_schemes));
}

Test(utils, is_valid_url_rejects_invalid_host)
{
    cr_assert_not(is_valid_url("http://", http_schemes));
    cr_assert_not(is_valid_url("http:// exam.com", http_schemes));
    cr_assert_not(is_valid_url("http://exam ple.com", http_schemes));
    cr_assert_not(is_valid_url("http://exam\tple.com", http_schemes));
}

Test(utils, is_valid_url_accepts_minimal_http_urls)
{
    cr_assert(is_valid_url("http://example.com", http_schemes));
    cr_assert(is_valid_url("https://example.com", http_schemes));
}

Test(utils, is_valid_url_accepts_paths_queries_and_fragments)
{
    cr_assert(is_valid_url("http://example.com/path/to/resource", http_schemes));
    cr_assert(is_valid_url("http://example.com?query=value", http_schemes));
    cr_assert(is_valid_url("http://example.com#fragment", http_schemes));
    cr_assert(is_valid_url("https://example.com/a/b?c=d#e", http_schemes));
}

Test(utils, is_valid_url_accepts_userinfo_and_port)
{
    cr_assert(is_valid_url("http://user:pass@example.com", http_schemes));
    cr_assert(is_valid_url("http://example.com:8080", http_schemes));
    cr_assert(is_valid_url("http://user@example.com:8080/path", http_schemes));
}

Test(utils, is_valid_url_accepts_rfc3986_host_characters)
{
    cr_assert(is_valid_url("http://sub.example.com", http_schemes));
    cr_assert(is_valid_url("http://127.0.0.1", http_schemes));
    cr_assert(is_valid_url("http://host-name.example:443", http_schemes));
}

Test(utils, is_valid_http_url_delegates_to_http_schemes)
{
    cr_assert(is_valid_http_url("http://example.com"));
    cr_assert(is_valid_http_url("https://example.com/path"));
    cr_assert_not(is_valid_http_url("ftp://example.com"));
    cr_assert_not(is_valid_http_url(NULL));
}

Test(utils, print_help_returns_success, .init = redirect_stdout)
{
    const char *messages[] = {
        "Avar",
        "Usage: avar [options]",
        "  --help  Show help",
    };

    cr_assert_eq(print_help(3, messages), EXIT_SUCCESS);
    cr_assert_stdout_eq_str(
        "Avar (v" TEST_VERSION_STR ")\n"
        "Usage: avar [options]\n"
        "  --help  Show help\n");
}

Test(utils, print_help_handles_single_line, .init = redirect_stdout)
{
    const char *messages[] = {"Avar"};

    cr_assert_eq(print_help(1, messages), EXIT_SUCCESS);
    cr_assert_stdout_eq_str("Avar (v" TEST_VERSION_STR ")\n");
}
