#ifndef AVAR_HTTP_PROXY_H
#define AVAR_HTTP_PROXY_H

#include <stdbool.h>
#include <stddef.h>

struct mg_connection;

/* Builds a proxy URL such as http://user:pass@host:port. Caller must free(). */
char *proxy_build_url(const char *type, const char *host, unsigned port,
                      const char *username, const char *password);

/* Loads global dm.proxy.* settings into a URL. Returns NULL when disabled. Caller must free(). */
char *proxy_load_global_url(void);

/* Parses JSON-RPC proxy params into a URL. Caller must free(). */
char *proxy_url_from_json(const void *proxy_obj);

/* Returns true when url points at an https resource. */
bool proxy_target_is_https(const char *url);

/* Sends an HTTP CONNECT tunnel request through a proxy connection. */
void proxy_send_connect(struct mg_connection *c, const char *target_url);

/* Returns true when a proxy CONNECT response indicates success. */
bool proxy_connect_response_ok(const char *buf, size_t len);

#endif
