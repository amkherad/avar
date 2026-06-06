#include <utils.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VERSION_STR
#include "avar.h"
#endif

/* ---------- helper predicates --------------------------------------- */
static bool is_scheme(const char *s, size_t len)
{
    return (len == 4 && memcmp(s, "http", 4) == 0) ||
           (len == 5 && memcmp(s, "https", 5) == 0);
}

static bool is_host_char(char c)
{
    /* RFC3986: unreserved | percent‑encoded | sub-delims | ":" */
    return isalnum((unsigned char)c) ||   /* a–z A–Z 0–9            */
           strchr("-._~!$&'()*+,;=:@", c);/* the rest of the allowed set */
}

static bool is_host(const char *h)
{
    if (!*h) return false;          /* empty host is illegal */

    const char *p = h;
    while (*p && *p != ':' && *p != '/' && *p != '?' && *p != '#')
    {
        if (!is_host_char(*p)) return false;
        ++p;
    }
    return true;
}

/* ---------- main routine --------------------------------------------- */
bool is_valid_http_url(stringa url) {
    static stringa httpSchema[] = {"http", "https"};
    return is_valid_url(url, httpSchema);
}

bool is_valid_url(stringa url, stringa validSchemes[])
{
    if (url == NULL) return false;

    /* 1. Find the scheme separator "://". */
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) return false;          /* missing scheme */

    size_t scheme_len = scheme_end - url;
    if (!is_scheme(url, scheme_len)) return false;

    /* 2. Authority starts after the "://". */
    const char *authority_start = scheme_end + 3;
    if (*authority_start == '\0') return false;   /* nothing after :// */

    /* 3. Find the end of the authority (first '/', '?' or '#'). */
    const char *authority_end =
        strpbrk(authority_start, "/?#");
    size_t auth_len = authority_end ? (size_t)(authority_end - authority_start)
                                    : strlen(authority_start);

    /* 4. Extract host part.  We ignore optional userinfo@ and port: */
    const char *host_start = authority_start;
    const char *user_at = strchr(host_start, '@');
    if (user_at) host_start = user_at + 1;           /* skip userinfo */

    const char *port_colon = strchr(host_start, ':');
    size_t host_len = port_colon ?
                      (size_t)(port_colon - host_start)
                    : auth_len;

    if (!is_host(host_start)) return false;

    /* 5. The rest (path / query / fragment) is optional and can be
       anything – no further validation is done here. */
    return true;
}

int print_help(int help_message_n, const char* help_message[]) {
    for (int i = 0; i < help_message_n; i++) {
        if (i == 0) {
            printf("%s (v%s)\n", help_message[i], VERSION_STR);
        } else {
            puts(help_message[i]);
        }
    }

    return EXIT_SUCCESS;
}
