#include "avar.h"
#include <download.h>
#include <download_probe.h>
#include <http.h>
#include <http_proxy.h>
#include <mongoose.h>
#include <utils.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_CONNECT_TIMEOUT_MS 15000U
#define PROBE_TOTAL_TIMEOUT_MS 30000U
#define PROBE_POLL_MS 50

typedef enum {
    ProbeMethodHead = 0,
    ProbeMethodRangeGet,
} ProbeMethod;

typedef struct {
    struct mg_mgr mgr;
    char *url;
    char *referer;
    char *proxy_url;
    ProbeMethod method;
    bool use_proxy;
    ProxyKind proxy_kind;
    bool proxy_http_forward;
    bool proxy_tunnel_done;
    ProxySocksCtx socks;
    bool request_sent;
    bool done;
    bool failed;
    int http_status;
    uint64_t total_bytes;
    uint64_t deadline_ms;
    uint64_t connect_deadline_ms;
} ProbeCtx;

static bool parse_content_range_total(const struct mg_str value, uint64_t *total_out) {
    if (total_out == NULL || value.buf == NULL) {
        return false;
    }

    const char *slash = memchr(value.buf, '/', value.len);
    if (slash == NULL) {
        return false;
    }

    const char *total_start = slash + 1;
    const size_t total_len = value.len - (size_t)(total_start - value.buf);
    char buffer[32];
    if (total_len >= sizeof buffer) {
        return false;
    }

    memcpy(buffer, total_start, total_len);
    buffer[total_len] = '\0';

    if (strcmp(buffer, "*") == 0) {
        return false;
    }

    char *end = NULL;
    const unsigned long long total = strtoull(buffer, &end, 10);
    if (end == buffer) {
        return false;
    }

    *total_out = (uint64_t)total;
    return true;
}

static bool parse_content_length(const struct mg_str *content_length, uint64_t *total_out) {
    if (content_length == NULL || content_length->len == 0U || total_out == NULL) {
        return false;
    }

    char buffer[32];
    if (content_length->len >= sizeof buffer) {
        return false;
    }

    memcpy(buffer, content_length->buf, content_length->len);
    buffer[content_length->len] = '\0';

    char *end = NULL;
    const unsigned long long len = strtoull(buffer, &end, 10);
    if (end == buffer) {
        return false;
    }

    *total_out = (uint64_t)len;
    return true;
}

static void probe_extract_size(ProbeCtx *ctx, struct mg_http_message *hm) {
    if (ctx == NULL || hm == NULL) {
        return;
    }

    struct mg_str *content_range = mg_http_get_header(hm, "Content-Range");
    if (content_range != NULL && parse_content_range_total(*content_range, &ctx->total_bytes)) {
        return;
    }

    struct mg_str *content_length = mg_http_get_header(hm, "Content-Length");
    (void)parse_content_length(content_length, &ctx->total_bytes);
}

static void probe_send_request(ProbeCtx *ctx, struct mg_connection *c) {
    if (ctx == NULL || c == NULL || ctx->request_sent) {
        return;
    }

    ctx->request_sent = true;

    const struct mg_str host = mg_url_host(ctx->url);
    const char *uri = mg_url_uri(ctx->url);
    if (ctx->use_proxy && ctx->proxy_http_forward) {
        uri = ctx->url;
    }

    const char *method = ctx->method == ProbeMethodHead ? "HEAD" : "GET";
    mg_printf(c,
              "%s %s HTTP/1.1\r\n"
              "Host: %.*s\r\n"
              "User-Agent: " APP_NAME "/" VERSION_STR "\r\n"
              "Accept: */*\r\n"
              "Accept-Encoding: identity\r\n"
              "Connection: close\r\n",
              method, uri, (int)host.len, host.buf);

    if (ctx->referer != NULL && ctx->referer[0] != '\0') {
        mg_printf(c, "Referer: %s\r\n", ctx->referer);
    }

    if (ctx->method == ProbeMethodRangeGet) {
        mg_printf(c, "Range: bytes=0-0\r\n");
    }

    mg_printf(c, "\r\n");
}

static void probe_init_tls(struct mg_connection *c, const char *url) {
    if (c == NULL || url == NULL) {
        return;
    }

    struct mg_str host = mg_url_host(url);
    if (host.len == 0U) {
        return;
    }

    char hostbuf[256];
    const size_t copy_len = host.len < sizeof hostbuf - 1U ? host.len : sizeof hostbuf - 1U;
    memcpy(hostbuf, host.buf, copy_len);
    hostbuf[copy_len] = '\0';

    struct mg_tls_opts opts = {.name = hostbuf};
    mg_tls_init(c, &opts);
}

static void probe_handler(struct mg_connection *c, int ev, void *ev_data) {
    ProbeCtx *ctx = (ProbeCtx *)c->fn_data;
    if (ctx == NULL || ctx->done) {
        return;
    }

    if (ev == MG_EV_POLL) {
        const uint64_t now = mg_millis();
        if (now > ctx->deadline_ms) {
            ctx->failed = true;
            ctx->done = true;
            c->is_closing = 1;
            return;
        }

        if (now > ctx->connect_deadline_ms && (c->is_connecting || c->is_resolving)) {
            ctx->failed = true;
            ctx->done = true;
            c->is_closing = 1;
        }
        return;
    }

    if (ev == MG_EV_OPEN) {
        ctx->connect_deadline_ms = mg_millis() + PROBE_CONNECT_TIMEOUT_MS;
        return;
    }

    if (ev == MG_EV_CONNECT) {
        c->data[0] = '\0';
        ctx->request_sent = false;

        if (ctx->use_proxy && !ctx->proxy_tunnel_done) {
            if (ctx->proxy_kind == ProxyKindSocks5) {
                proxy_socks5_begin(c, ctx->url, ctx->proxy_url, &ctx->socks);
                return;
            }
            if (proxy_target_is_https(ctx->url)) {
                proxy_send_connect(c, ctx->url, ctx->proxy_url);
                return;
            }
            probe_send_request(ctx, c);
            return;
        }

        if (c->is_tls) {
            probe_init_tls(c, ctx->url);
        } else {
            probe_send_request(ctx, c);
        }
        return;
    }

    if (ev == MG_EV_TLS_HS) {
        probe_send_request(ctx, c);
        return;
    }

    if (ev == MG_EV_WRITE) {
        if (!ctx->request_sent && !c->is_connecting && !c->is_tls_hs) {
            probe_send_request(ctx, c);
        }
        return;
    }

    if (ev == MG_EV_READ && ctx->use_proxy && !ctx->proxy_tunnel_done && c->recv.len > 0U) {
        if (ctx->proxy_kind == ProxyKindSocks5) {
            if (proxy_socks5_handle_read((const char *)c->recv.buf, c->recv.len, c, ctx->url,
                                         ctx->proxy_url, &ctx->socks)) {
                ctx->proxy_tunnel_done = true;
                c->recv.len = 0U;
                if (proxy_target_is_https(ctx->url)) {
                    probe_init_tls(c, ctx->url);
                } else {
                    probe_send_request(ctx, c);
                }
                return;
            }
            if (ctx->socks.stage == ProxySocksStageFailed) {
                ctx->failed = true;
                ctx->done = true;
                c->is_closing = 1;
            }
            return;
        }

        if (proxy_connect_response_ok((const char *)c->recv.buf, c->recv.len)) {
            ctx->proxy_tunnel_done = true;
            c->recv.len = 0U;
            if (proxy_target_is_https(ctx->url)) {
                probe_init_tls(c, ctx->url);
            } else {
                probe_send_request(ctx, c);
            }
        }
        return;
    }

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        ctx->http_status = mg_http_status(hm);
        probe_extract_size(ctx, hm);

        ctx->done = true;
        c->is_closing = 1;
        return;
    }

    if (ev == MG_EV_ERROR) {
        ctx->failed = true;
        ctx->done = true;
    }

    if (ev == MG_EV_CLOSE) {
        ctx->done = true;
    }
}

static char *probe_resolve_proxy(const char *url, const char *proxy_url,
                                 const char *download_id) {
    if (proxy_url != NULL && proxy_url[0] != '\0') {
        return strdup(proxy_url);
    }

    if (download_id == NULL || download_id[0] == '\0') {
        return proxy_resolve_for_target(url, NULL);
    }

    DownloadState *state = download_item_state_load(download_id);
    if (state == NULL) {
        return proxy_resolve_for_target(url, NULL);
    }

    char *resolved = proxy_resolve_for_target(url, state->proxy);
    download_state_free(state);
    return resolved;
}

static int probe_run_once(ProbeCtx *ctx) {
    ctx->request_sent = false;
    ctx->done = false;
    ctx->failed = false;
    ctx->http_status = 0;
    ctx->total_bytes = 0U;
    ctx->proxy_tunnel_done = false;
    ctx->socks = (ProxySocksCtx){.stage = ProxySocksStageNone};
    ctx->deadline_ms = mg_millis() + PROBE_TOTAL_TIMEOUT_MS;

    ctx->use_proxy = ctx->proxy_url != NULL;
    if (ctx->use_proxy) {
        ctx->proxy_kind = proxy_kind_from_url(ctx->proxy_url);
        ctx->proxy_http_forward = proxy_uses_http_forward(ctx->proxy_url, ctx->url);
    }

    char *proxy_connect = ctx->use_proxy ? proxy_connect_url(ctx->proxy_url) : NULL;
    if (ctx->use_proxy && proxy_connect == NULL) {
        return EXIT_FAILURE;
    }

    const char *resolved_connect = proxy_connect != NULL ? proxy_connect : ctx->url;
    struct mg_connection *c = mg_http_connect(&ctx->mgr, resolved_connect, probe_handler, ctx);
    free(proxy_connect);
    if (c == NULL) {
        return EXIT_FAILURE;
    }

    while (!ctx->done) {
        mg_mgr_poll(&ctx->mgr, (int)PROBE_POLL_MS);
    }

    return ctx->failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

int download_probe_url(const char *url, const char *referer, const char *proxy_url,
                       const char *download_id, DownloadProbeResult *out) {
    if (out == NULL) {
        return EXIT_FAILURE;
    }

    out->exit_code = EXIT_FAILURE;
    out->http_status = 0;
    out->total_bytes = 0U;

    if (url == NULL) {
        return EXIT_FAILURE;
    }

    char *normalized = strdup(url);
    if (normalized == NULL) {
        return EXIT_FAILURE;
    }
    trim_whitespace_inplace(normalized);

    if (!is_valid_http_url(normalized)) {
        free(normalized);
        return EXIT_FAILURE;
    }

    ProbeCtx ctx = {0};
    ctx.url = normalized;
    if (referer != NULL && referer[0] != '\0') {
        ctx.referer = strdup(referer);
        if (ctx.referer == NULL) {
            free(normalized);
            return EXIT_FAILURE;
        }
    }

    ctx.proxy_url = probe_resolve_proxy(normalized, proxy_url, download_id);
    ctx.method = ProbeMethodHead;

    mg_mgr_init(&ctx.mgr);

    int rc = probe_run_once(&ctx);
    if (rc == EXIT_SUCCESS && ctx.total_bytes == 0U && ctx.http_status >= 200
        && ctx.http_status < 300) {
        mg_mgr_free(&ctx.mgr);
        mg_mgr_init(&ctx.mgr);
        ctx.method = ProbeMethodRangeGet;
        rc = probe_run_once(&ctx);
    }

    mg_mgr_free(&ctx.mgr);

    out->exit_code = rc;
    out->http_status = ctx.http_status;
    out->total_bytes = ctx.total_bytes;

    free(ctx.referer);
    free(ctx.proxy_url);
    free(ctx.url);
    return rc;
}
