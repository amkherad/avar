#ifndef TLS_DEBUG_H
#define TLS_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

struct mg_connection;

void tls_apply_client_policy(struct mg_connection *c);

void tls_log_handshake_start(const char *url, const char *sni_host);

void tls_log_download_error(struct mg_connection *c, const char *url, const char *item_id,
                            const char *proxy_url, uint64_t range_start, uint64_t range_end,
                            bool has_range, const char *message);

#endif
