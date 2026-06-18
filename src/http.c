#include "avar.h"
#include <http.h>
#include <mongoose.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int strncasecmp_local(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const unsigned char ca = (unsigned char)a[i];
        const unsigned char cb = (unsigned char)b[i];
        if (ca == '\0' || cb == '\0') {
            return (int)ca - (int)cb;
        }
        const unsigned char la = (unsigned char)tolower(ca);
        const unsigned char lb = (unsigned char)tolower(cb);
        if (la != lb) {
            return (int)la - (int)lb;
        }
    }
    return 0;
}

static char *dup_n(const char *start, size_t len) {
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static void trim_quotes(char *value) {
    if (value == NULL) {
        return;
    }

    const size_t len = strlen(value);
    if (len >= 2 && ((value[0] == '"' && value[len - 1] == '"')
                     || (value[0] == '\'' && value[len - 1] == '\''))) {
        memmove(value, value + 1, len - 2);
        value[len - 2] = '\0';
    }
}

static void trim_whitespace(char *value) {
    if (value == NULL || value[0] == '\0') {
        return;
    }

    char *start = value;
    while (*start != '\0' && isspace((unsigned char) *start)) {
        start++;
    }

    if (start != value) {
        memmove(value, start, strlen(start) + 1);
    }

    size_t len = strlen(value);
    while (len > 0 && isspace((unsigned char) value[len - 1])) {
        value[--len] = '\0';
    }
}

char *http_url_basename(const char *url) {
    if (url == NULL) {
        return NULL;
    }

    const char *path = strstr(url, "://");
    if (path != NULL) {
        path += 3;
        const char *slash = strchr(path, '/');
        if (slash == NULL || slash[1] == '\0') {
            return NULL;
        }
        path = slash + 1;
    } else {
        path = url;
    }

    const char *query = strpbrk(path, "?#");
    const char *end = path + strlen(path);
    if (query != NULL && query < end) {
        end = query;
    }

    const char *slash = end;
    while (slash > path && slash[-1] != '/') {
        slash--;
    }

    if (slash == end) {
        return NULL;
    }

    const size_t len = (size_t)(end - slash);
    if (len == 0) {
        return NULL;
    }

    char *encoded = dup_n(slash, len);
    if (encoded == NULL) {
        return NULL;
    }

    char decoded[AVAR_HTTP_DECODE_BUFFER];
    if (mg_url_decode(encoded, strlen(encoded), decoded, sizeof decoded, 0) <= 0) {
        free(encoded);
        return NULL;
    }
    free(encoded);

    return strdup(decoded);
}

char *http_parse_content_disposition_filename(const char *value, const size_t len) {
    if (value == NULL || len == 0) {
        return NULL;
    }

    const char *start = value;
    const char *end = value + len;

    for (const char *p = start; p < end; p++) {
        if ((p + 8 < end && strncasecmp_local(p, "filename", 8) == 0)
            || (p + 16 < end && strncasecmp_local(p, "filename*", 9) == 0)) {
            const bool rfc5987 = strncasecmp_local(p, "filename*", 9) == 0;
            const char *eq = strchr(p, '=');
            if (eq == NULL || eq >= end) {
                continue;
            }

            const char *name_start = eq + 1;
            while (name_start < end && isspace((unsigned char)*name_start)) {
                name_start++;
            }

            if (rfc5987 && (size_t)(end - name_start) > 7
                && strncasecmp_local(name_start, "utf-8''", 7) == 0) {
                name_start += 7;
            }

            const char *name_end = end;
            for (const char *q = name_start; q < end; q++) {
                if (*q == ';') {
                    name_end = q;
                    break;
                }
            }

            while (name_end > name_start && isspace((unsigned char)name_end[-1])) {
                name_end--;
            }

            char *filename = dup_n(name_start, (size_t)(name_end - name_start));
            if (filename == NULL) {
                return NULL;
            }
            trim_quotes(filename);

            if (rfc5987) {
                char decoded[AVAR_HTTP_DECODE_BUFFER];
                if (mg_url_decode(filename, strlen(filename), decoded, sizeof decoded, 0) > 0) {
                    char *decoded_copy = strdup(decoded);
                    free(filename);
                    filename = decoded_copy;
                    if (filename == NULL) {
                        return NULL;
                    }
                }
            }

            return filename;
        }
    }

    return NULL;
}

char *http_resolve_url(const char *base_url, const char *location, const size_t location_len) {
    if (base_url == NULL || location == NULL || location_len == 0) {
        return NULL;
    }

    char *loc = dup_n(location, location_len);
    if (loc == NULL) {
        return NULL;
    }

    trim_whitespace(loc);

    if (strncmp(loc, "http://", 7) == 0 || strncmp(loc, "https://", 8) == 0) {
        return loc;
    }

    if (strncmp(loc, "//", 2) == 0) {
        const char *scheme_end = strstr(base_url, "://");
        if (scheme_end == NULL) {
            free(loc);
            return NULL;
        }

        const size_t scheme_len = (size_t) (scheme_end - base_url);
        const size_t absolute_len = scheme_len + 1 + strlen(loc) + 1;
        char *absolute = malloc(absolute_len);
        if (absolute == NULL) {
            free(loc);
            return NULL;
        }

        snprintf(absolute, absolute_len, "%.*s:%s", (int) scheme_len, base_url, loc);
        free(loc);
        return absolute;
    }

    if (loc[0] == '/') {
        struct mg_str host = mg_url_host(base_url);
        const char *scheme_end = strstr(base_url, "://");
        if (scheme_end == NULL) {
            free(loc);
            return NULL;
        }

        const size_t scheme_len = (size_t)(scheme_end - base_url);
        char *absolute = malloc(scheme_len + 3 + host.len + strlen(loc) + 1);
        if (absolute == NULL) {
            free(loc);
            return NULL;
        }

        snprintf(absolute, scheme_len + 3 + host.len + strlen(loc) + 1, "%.*s://%.*s%s",
                 (int)scheme_len, base_url, (int)host.len, host.buf, loc);
        free(loc);
        return absolute;
    }

    const char *scheme_end = strstr(base_url, "://");
    if (scheme_end == NULL) {
        free(loc);
        return NULL;
    }

    const char *path_start = strchr(scheme_end + 3, '/');
    if (path_start == NULL) {
        char *absolute = malloc(strlen(base_url) + 1 + strlen(loc) + 1);
        if (absolute == NULL) {
            free(loc);
            return NULL;
        }
        snprintf(absolute, strlen(base_url) + 1 + strlen(loc) + 1, "%s/%s", base_url, loc);
        free(loc);
        return absolute;
    }

    const size_t path_offset = (size_t) (path_start - base_url);

    char *base_copy = strdup(base_url);
    if (base_copy == NULL) {
        free(loc);
        return NULL;
    }

    char *last_slash = strrchr(base_copy, '/');
    if (last_slash != NULL && last_slash >= base_copy + path_offset) {
        last_slash[1] = '\0';
    }

    char *absolute = malloc(strlen(base_copy) + strlen(loc) + 2);
    if (absolute == NULL) {
        free(base_copy);
        free(loc);
        return NULL;
    }

    snprintf(absolute, strlen(base_copy) + strlen(loc) + 2, "%s%s", base_copy, loc);
    free(base_copy);
    free(loc);
    return absolute;
}

bool http_is_redirect(const int status) {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}
