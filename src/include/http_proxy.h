#ifndef AVAR_HTTP_PROXY_H
#define AVAR_HTTP_PROXY_H

#include <stdbool.h>
#include <stddef.h>

struct mg_connection;

typedef enum {
    ProxyKindNone = 0,
    ProxyKindHttp,
    ProxyKindHttps,
    ProxyKindSocks5,
} ProxyKind;

typedef enum {
    ProxySocksStageNone = 0,
    ProxySocksStageGreetingSent,
    ProxySocksStageAuthSent,
    ProxySocksStageConnectSent,
    ProxySocksStageDone,
    ProxySocksStageFailed,
} ProxySocksStage;

typedef struct ProxySocksCtx {
    ProxySocksStage stage;
    char username[256];
    char password[256];
    bool has_auth;
} ProxySocksCtx;

/* Builds a proxy URL such as http://user:pass@host:port. Caller must free(). */
char *proxy_build_url(const char *type, const char *host, unsigned port,
                      const char *username, const char *password);

/* Normalizes user-provided proxy URLs (adds scheme when missing). Caller must free(). */
char *proxy_normalize_url(const char *url);

/* Returns the proxy kind for a URL, or ProxyKindNone when unknown. */
ProxyKind proxy_kind_from_url(const char *proxy_url);

/* Returns a connect URL suitable for mg_http_connect(). Caller must free(). */
char *proxy_connect_url(const char *proxy_url);

/* Loads global dm.proxy.* settings into a URL. Returns NULL when disabled. Caller must free(). */
char *proxy_load_global_url(void);

/* Loads proxy URL from HTTP_PROXY/HTTPS_PROXY/ALL_PROXY for target_url. Caller must free(). */
char *proxy_load_from_environment(const char *target_url);

/* Returns the NO_PROXY list from env or config. Caller must free(). May return NULL. */
char *proxy_get_no_proxy_list(void);

/* Returns true when target_url should bypass proxy per NO_PROXY rules. */
bool proxy_should_bypass(const char *target_url, const char *no_proxy_list);

/**
 * Resolves the proxy URL for a download target.
 * override_url wins when non-empty; otherwise env then global config apply.
 * Returns NULL when no proxy or bypassed. Caller must free().
 */
char *proxy_resolve_for_target(const char *target_url, const char *override_url);

/* Parses JSON-RPC proxy params or a URL string into a URL. Caller must free(). */
char *proxy_url_from_json(const void *proxy_obj);

/* Returns true when url points at an https resource. */
bool proxy_target_is_https(const char *url);

/* Returns true when requests through this proxy must use absolute URLs (HTTP forward). */
bool proxy_uses_http_forward(const char *proxy_url, const char *target_url);

/* Sends an HTTP CONNECT tunnel request through a proxy connection. */
void proxy_send_connect(struct mg_connection *c, const char *target_url, const char *proxy_url);

/* Returns true when a proxy CONNECT response indicates success. */
bool proxy_connect_response_ok(const char *buf, size_t len);

void proxy_socks5_begin(struct mg_connection *c, const char *target_url, const char *proxy_url,
                      ProxySocksCtx *ctx);

bool proxy_socks5_handle_read(const char *buf, size_t len, struct mg_connection *c,
                            const char *target_url, const char *proxy_url, ProxySocksCtx *ctx);

#endif
