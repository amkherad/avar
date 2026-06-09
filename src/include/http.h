#ifndef AVAR_HTTP_H
#define AVAR_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Extracts the path component from an HTTP(S) URL. Caller must free(). */
char *http_url_basename(const char *url);

/* Parses filename= from a Content-Disposition header value. Caller must free(). */
char *http_parse_content_disposition_filename(const char *value, size_t len);

/* Resolves a possibly relative Location header against base_url. Caller must free(). */
char *http_resolve_url(const char *base_url, const char *location, size_t location_len);

/* Returns true for redirect status codes that carry a Location header. */
bool http_is_redirect(int status);

#endif
