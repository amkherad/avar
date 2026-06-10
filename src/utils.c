#include "avar.h"
#include <utils.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>

/* ---------- helper predicates --------------------------------------- */
static bool is_scheme(const char *s, size_t len) {
    return (len == 4 && memcmp(s, "http", 4) == 0) ||
           (len == 5 && memcmp(s, "https", 5) == 0);
}

static bool is_host_char(char c) {
    /* RFC3986: unreserved | percent‑encoded | sub-delims | ":" */
    return isalnum((unsigned char) c) || /* a–z A–Z 0–9            */
           strchr("-._~!$&'()*+,;=:@", c); /* the rest of the allowed set */
}

static bool is_host(const char *h) {
    if (!*h) return false; /* empty host is illegal */

    const char *p = h;
    while (*p && *p != ':' && *p != '/' && *p != '?' && *p != '#') {
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

bool is_valid_url(stringa url, stringa validSchemes[]) {
    if (url == NULL) return false;

    /* 1. Find the scheme separator "://". */
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) return false; /* missing scheme */

    size_t scheme_len = scheme_end - url;
    if (!is_scheme(url, scheme_len)) return false;

    /* 2. Authority starts after the "://". */
    const char *authority_start = scheme_end + 3;
    if (*authority_start == '\0') return false; /* nothing after :// */

    /* 3. Find the end of the authority (first '/', '?' or '#'). */
    const char *authority_end =
            strpbrk(authority_start, "/?#");
    size_t auth_len = authority_end
                          ? (size_t) (authority_end - authority_start)
                          : strlen(authority_start);

    /* 4. Extract host part.  We ignore optional userinfo@ and port: */
    const char *host_start = authority_start;
    const char *user_at = strchr(host_start, '@');
    if (user_at) host_start = user_at + 1; /* skip userinfo */

    const char *port_colon = strchr(host_start, ':');
    size_t host_len = port_colon
                          ? (size_t) (port_colon - host_start)
                          : auth_len;

    if (!is_host(host_start)) return false;

    /* 5. The rest (path / query / fragment) is optional and can be
       anything – no further validation is done here. */
    return true;
}

char *strndup(const char *s, size_t n) {
    if (s == NULL) {
        return NULL;
    }

    size_t len = strlen(s);
    if (n < len) {
        len = n;
    }

    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

static bool str_ieq(const char *a, const char *b) {
    if (a == NULL || b == NULL) {
        return false;
    }

    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char) *a) != tolower((unsigned char) *b)) {
            return false;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static void format_scaled_value(double value, const char *suffix, char *buf, const size_t buflen) {
    if (value < 10.0 && value != floor(value)) {
        snprintf(buf, buflen, "%.1f %s", value, suffix);
    } else {
        snprintf(buf, buflen, "%.0f %s", value, suffix);
    }
}

static void format_scaled_value_padded(const double value, const char *suffix,
                                       const int number_width, char *buf, const size_t buflen) {
    if (value < 10.0 && value != floor(value)) {
        snprintf(buf, buflen, "%*.*f %s", number_width, 1, value, suffix);
    } else {
        snprintf(buf, buflen, "%*.*f %s", number_width, 0, value, suffix);
    }
}

bool avar_size_unit_parse(const char *text, AvarSizeUnit *out) {
    if (out == NULL) {
        return false;
    }

    if (text == NULL || text[0] == '\0') {
        *out = AVAR_SIZE_MIB;
        return true;
    }

    if (str_ieq(text, "bytes") || str_ieq(text, "b")) {
        *out = AVAR_SIZE_BYTES;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_KIB) == 0) {
        *out = AVAR_SIZE_KIB;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_MIB) == 0) {
        *out = AVAR_SIZE_MIB;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_GIB) == 0) {
        *out = AVAR_SIZE_GIB;
        return true;
    }

    return false;
}

bool avar_speed_unit_parse(const char *text, AvarSpeedUnit *out) {
    if (out == NULL) {
        return false;
    }

    if (text == NULL || text[0] == '\0') {
        *out = AVAR_SPEED_MIBIT_PER_SEC;
        return true;
    }

    if (str_ieq(text, AVAR_UNIT_BYTES_PER_SEC)) {
        *out = AVAR_SPEED_BYTES_PER_SEC;
        return true;
    }
    if (str_ieq(text, AVAR_UNIT_BITS_PER_SEC)) {
        *out = AVAR_SPEED_BITS_PER_SEC;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_KIB_PER_SEC) == 0) {
        *out = AVAR_SPEED_KIB_PER_SEC;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_MIB_PER_SEC) == 0) {
        *out = AVAR_SPEED_MIB_PER_SEC;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_GIB_PER_SEC) == 0) {
        *out = AVAR_SPEED_GIB_PER_SEC;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_KIBIT_PER_SEC) == 0) {
        *out = AVAR_SPEED_KIBIT_PER_SEC;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_MIBIT_PER_SEC) == 0) {
        *out = AVAR_SPEED_MIBIT_PER_SEC;
        return true;
    }
    if (strcmp(text, AVAR_UNIT_GIBIT_PER_SEC) == 0) {
        *out = AVAR_SPEED_GIBIT_PER_SEC;
        return true;
    }

    return false;
}

static double bytes_to_scaled_value(const uint64_t bytes, const AvarSizeUnit unit) {
    switch (unit) {
    case AVAR_SIZE_BYTES:
        return (double) bytes;
    case AVAR_SIZE_KIB:
        return (double) bytes / (double) AVAR_KIB;
    case AVAR_SIZE_MIB:
        return (double) bytes / (double) AVAR_MIB;
    case AVAR_SIZE_GIB:
        return (double) bytes / (double) AVAR_GIB;
    }

    return (double) bytes;
}

int avar_data_size_number_width(const uint64_t bytes, const AvarSizeUnit unit) {
    if (unit == AVAR_SIZE_BYTES) {
        return snprintf(NULL, 0, "%llu", (unsigned long long) bytes);
    }

    const double value = bytes_to_scaled_value(bytes, unit);
    if (value < 10.0 && value != floor(value)) {
        return snprintf(NULL, 0, "%.1f", value);
    }

    return snprintf(NULL, 0, "%.0f", value);
}

char *format_progress_percent(const int percent, char *buf, const size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return buf;
    }

    snprintf(buf, buflen, "%*d%%", DL_PROGRESS_PERCENT_WIDTH, percent);
    return buf;
}

char *format_data_size_padded(const uint64_t bytes, const AvarSizeUnit unit,
                              const int number_width, char *buf, const size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return buf;
    }

    switch (unit) {
    case AVAR_SIZE_BYTES:
        snprintf(buf, buflen, "%*llu %s", number_width, (unsigned long long) bytes,
                 AVAR_UNIT_BYTES);
        break;
    case AVAR_SIZE_KIB:
        format_scaled_value_padded((double) bytes / (double) AVAR_KIB, AVAR_UNIT_KIB, number_width,
                                   buf, buflen);
        break;
    case AVAR_SIZE_MIB:
        format_scaled_value_padded((double) bytes / (double) AVAR_MIB, AVAR_UNIT_MIB, number_width,
                                   buf, buflen);
        break;
    case AVAR_SIZE_GIB:
        format_scaled_value_padded((double) bytes / (double) AVAR_GIB, AVAR_UNIT_GIB, number_width,
                                   buf, buflen);
        break;
    }

    return buf;
}

char *format_data_size(const uint64_t bytes, const AvarSizeUnit unit, char *buf,
                       const size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return buf;
    }

    switch (unit) {
    case AVAR_SIZE_BYTES:
        snprintf(buf, buflen, "%llu %s", (unsigned long long) bytes, AVAR_UNIT_BYTES);
        break;
    case AVAR_SIZE_KIB:
        format_scaled_value((double) bytes / (double) AVAR_KIB, AVAR_UNIT_KIB, buf, buflen);
        break;
    case AVAR_SIZE_MIB:
        format_scaled_value((double) bytes / (double) AVAR_MIB, AVAR_UNIT_MIB, buf, buflen);
        break;
    case AVAR_SIZE_GIB:
        format_scaled_value((double) bytes / (double) AVAR_GIB, AVAR_UNIT_GIB, buf, buflen);
        break;
    }

    return buf;
}

char *format_transfer_rate_padded(const double bytes_per_sec, const AvarSpeedUnit unit,
                                  const int number_width, char *buf, const size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return buf;
    }

    switch (unit) {
    case AVAR_SPEED_BYTES_PER_SEC:
        format_scaled_value_padded(bytes_per_sec, AVAR_UNIT_BYTES_PER_SEC, number_width, buf,
                                   buflen);
        break;
    case AVAR_SPEED_BITS_PER_SEC:
        format_scaled_value_padded(bytes_per_sec * (double) AVAR_BITS_PER_BYTE,
                                 AVAR_UNIT_BITS_PER_SEC, number_width, buf, buflen);
        break;
    case AVAR_SPEED_KIB_PER_SEC:
        format_scaled_value_padded(bytes_per_sec / (double) AVAR_KIB, AVAR_UNIT_KIB_PER_SEC,
                                   number_width, buf, buflen);
        break;
    case AVAR_SPEED_MIB_PER_SEC:
        format_scaled_value_padded(bytes_per_sec / (double) AVAR_MIB, AVAR_UNIT_MIB_PER_SEC,
                                   number_width, buf, buflen);
        break;
    case AVAR_SPEED_GIB_PER_SEC:
        format_scaled_value_padded(bytes_per_sec / (double) AVAR_GIB, AVAR_UNIT_GIB_PER_SEC,
                                   number_width, buf, buflen);
        break;
    case AVAR_SPEED_KIBIT_PER_SEC:
        format_scaled_value_padded((bytes_per_sec * (double) AVAR_BITS_PER_BYTE) / (double) AVAR_KIB,
                                   AVAR_UNIT_KIBIT_PER_SEC, number_width, buf, buflen);
        break;
    case AVAR_SPEED_MIBIT_PER_SEC:
        format_scaled_value_padded((bytes_per_sec * (double) AVAR_BITS_PER_BYTE) / (double) AVAR_MIB,
                                   AVAR_UNIT_MIBIT_PER_SEC, number_width, buf, buflen);
        break;
    case AVAR_SPEED_GIBIT_PER_SEC:
        format_scaled_value_padded((bytes_per_sec * (double) AVAR_BITS_PER_BYTE) / (double) AVAR_GIB,
                                   AVAR_UNIT_GIBIT_PER_SEC, number_width, buf, buflen);
        break;
    }

    return buf;
}

char *format_transfer_rate(const double bytes_per_sec, const AvarSpeedUnit unit, char *buf,
                           const size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return buf;
    }

    switch (unit) {
    case AVAR_SPEED_BYTES_PER_SEC:
        format_scaled_value(bytes_per_sec, AVAR_UNIT_BYTES_PER_SEC, buf, buflen);
        break;
    case AVAR_SPEED_BITS_PER_SEC:
        format_scaled_value(bytes_per_sec * (double) AVAR_BITS_PER_BYTE, AVAR_UNIT_BITS_PER_SEC, buf,
                            buflen);
        break;
    case AVAR_SPEED_KIB_PER_SEC:
        format_scaled_value(bytes_per_sec / (double) AVAR_KIB, AVAR_UNIT_KIB_PER_SEC, buf, buflen);
        break;
    case AVAR_SPEED_MIB_PER_SEC:
        format_scaled_value(bytes_per_sec / (double) AVAR_MIB, AVAR_UNIT_MIB_PER_SEC, buf, buflen);
        break;
    case AVAR_SPEED_GIB_PER_SEC:
        format_scaled_value(bytes_per_sec / (double) AVAR_GIB, AVAR_UNIT_GIB_PER_SEC, buf, buflen);
        break;
    case AVAR_SPEED_KIBIT_PER_SEC:
        format_scaled_value((bytes_per_sec * (double) AVAR_BITS_PER_BYTE) / (double) AVAR_KIB,
                            AVAR_UNIT_KIBIT_PER_SEC, buf, buflen);
        break;
    case AVAR_SPEED_MIBIT_PER_SEC:
        format_scaled_value((bytes_per_sec * (double) AVAR_BITS_PER_BYTE) / (double) AVAR_MIB,
                            AVAR_UNIT_MIBIT_PER_SEC, buf, buflen);
        break;
    case AVAR_SPEED_GIBIT_PER_SEC:
        format_scaled_value((bytes_per_sec * (double) AVAR_BITS_PER_BYTE) / (double) AVAR_GIB,
                            AVAR_UNIT_GIBIT_PER_SEC, buf, buflen);
        break;
    }

    return buf;
}

char *format_progress_bar(const int percent, const int width, char *buf, const size_t buflen) {
    if (buf == NULL || buflen == 0 || width <= 0) {
        return buf;
    }

    const size_t need = (size_t) width + 3;
    if (buflen < need) {
        buf[0] = '\0';
        return buf;
    }

    int clamped = percent;
    if (clamped < 0) {
        clamped = 0;
    } else if (clamped > 100) {
        clamped = 100;
    }

    const int filled = (clamped * width) / 100;
    buf[0] = '[';
    for (int i = 0; i < width; ++i) {
        buf[(size_t) i + 1] = i < filled ? '=' : ' ';
    }
    buf[(size_t) width + 1] = ']';
    buf[(size_t) width + 2] = '\0';
    return buf;
}

int print_help(int help_message_n, const char *help_message[]) {
    for (int i = 0; i < help_message_n; i++) {
        if (i == 0) {
            printf("%s (v%s)\n", help_message[i], VERSION_STR);
        } else {
            puts(help_message[i]);
        }
    }

    return EXIT_SUCCESS;
}

// char *mg_addr_to_string(const struct mg_addr *addr, char *buf, size_t buf_size) {
//     if (addr == NULL || buf == NULL || buf_size == 0) {
//         return NULL;
//     }
//
//     if (addr->is_ip6) {
//         if (inet_ntop(AF_INET6, addr->addr.ip, buf, buf_size) == NULL) {
//             return NULL;
//         }
//     } else {
//         if (inet_ntop(AF_INET, &addr->addr.ip4, buf, buf_size) == NULL) {
//             return NULL;
//         }
//     }
//
//     return buf;
// }
