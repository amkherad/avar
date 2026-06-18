#include <stream_hls.h>
#include <http.h>

#include <mbedtls/aes.h>

#include <mongoose.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HLS_FETCH_TIMEOUT_MS 60000U
#define HLS_MAX_BODY (64U * 1024U * 1024U)

typedef struct {
    struct mg_mgr mgr;
    bool done;
    bool ok;
    int status;
    bool request_sent;
    char *body;
    size_t body_len;
    const char *url;
    const char *referer;
} HttpFetchCtx;

typedef struct {
    char *uri;
    char *key_uri;
    unsigned char iv[16];
    bool iv_set;
    bool encrypted;
} HlsSegment;

typedef struct {
    HlsSegment *items;
    size_t count;
    size_t capacity;
} HlsPlaylist;

static bool str_case_equal(const char *a, const char *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

bool stream_url_is_hls(const char *url, const char *stream_kind) {
    if (stream_kind != NULL && str_case_equal(stream_kind, "hls")) {
        return true;
    }
    return url != NULL && (strstr(url, ".m3u8") != NULL || strstr(url, ".M3U8") != NULL);
}

static void http_fetch_send(struct mg_connection *c, HttpFetchCtx *ctx) {
    if (ctx == NULL || ctx->request_sent) {
        return;
    }
    ctx->request_sent = true;

    const struct mg_str host = mg_url_host(ctx->url);
    const char *uri = mg_url_uri(ctx->url);

    mg_printf(c,
              "GET %s HTTP/1.1\r\n"
              "Host: %.*s\r\n"
              "User-Agent: Avar\r\n"
              "Accept: */*\r\n"
              "Accept-Encoding: identity\r\n"
              "Connection: close\r\n",
              uri, (int)host.len, host.buf);

    if (ctx->referer != NULL && ctx->referer[0] != '\0') {
        mg_printf(c, "Referer: %s\r\n", ctx->referer);
    }
    mg_printf(c, "\r\n");
}

static void http_fetch_handler(struct mg_connection *c, int ev, void *ev_data) {
    HttpFetchCtx *ctx = (HttpFetchCtx *)c->fn_data;
    if (ctx == NULL) {
        return;
    }

    if (ev == MG_EV_CONNECT) {
        if (ev_data != NULL && *(int *)ev_data != 0) {
            ctx->done = true;
            c->is_closing = 1;
        }
        return;
    }

    if (ev == MG_EV_TLS_HS) {
        http_fetch_send(c, ctx);
        return;
    }

    if (ev == MG_EV_WRITE) {
        if (!ctx->request_sent && !c->is_connecting && !c->is_tls_hs) {
            http_fetch_send(c, ctx);
        }
        return;
    }

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        ctx->status = mg_http_status(hm);
        if (ctx->status >= 200 && ctx->status < 300 && hm->body.len > 0U && hm->body.len <= HLS_MAX_BODY) {
            ctx->body = malloc(hm->body.len + 1U);
            if (ctx->body != NULL) {
                memcpy(ctx->body, hm->body.buf, hm->body.len);
                ctx->body[hm->body.len] = '\0';
                ctx->body_len = hm->body.len;
                ctx->ok = true;
            }
        }
        ctx->done = true;
        c->is_closing = 1;
    }
}

static char *http_fetch_body(const char *url, const char *referer, size_t *out_len) {
    if (out_len != NULL) {
        *out_len = 0U;
    }
    if (url == NULL) {
        return NULL;
    }

    HttpFetchCtx ctx = {.url = url, .referer = referer};
    mg_mgr_init(&ctx.mgr);
    mg_log_set(MG_LL_ERROR);

    struct mg_connection *c = mg_http_connect(&ctx.mgr, url, http_fetch_handler, &ctx);
    if (c == NULL) {
        mg_mgr_free(&ctx.mgr);
        return NULL;
    }

#if defined(MG_TLS)
    if (mg_url_is_ssl(url)) {
        struct mg_tls_opts opts = {.name = mg_url_host(url)};
        mg_tls_init(c, &opts);
    }
#endif

    const uint64_t deadline = mg_millis() + (uint64_t)HLS_FETCH_TIMEOUT_MS;
    while (!ctx.done && mg_millis() < deadline) {
        mg_mgr_poll(&ctx.mgr, 50);
    }

    mg_mgr_free(&ctx.mgr);
    if (!ctx.ok || ctx.body == NULL) {
        free(ctx.body);
        return NULL;
    }

    if (out_len != NULL) {
        *out_len = ctx.body_len;
    }
    return ctx.body;
}

static char *trim_line(char *line) {
    if (line == NULL) {
        return NULL;
    }
    size_t len = strlen(line);
    while (len > 0U
           && (line[len - 1U] == '\r' || line[len - 1U] == '\n'
               || isspace((unsigned char)line[len - 1U]))) {
        line[--len] = '\0';
    }
    char *start = line;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != line) {
        memmove(line, start, strlen(start) + 1U);
    }
    return line;
}

static bool parse_hex_iv(const char *value, unsigned char iv[16]) {
    if (value == NULL) {
        return false;
    }
    if (strncmp(value, "0x", 2) == 0 || strncmp(value, "0X", 2) == 0) {
        value += 2;
    }
    if (strlen(value) != 32U) {
        return false;
    }
    for (size_t i = 0U; i < 16U; i++) {
        unsigned int byte = 0U;
        if (sscanf(value + (i * 2U), "%2x", &byte) != 1) {
            return false;
        }
        iv[i] = (unsigned char)byte;
    }
    return true;
}

static char *dup_slice(const char *start, size_t len) {
    char *out = malloc(len + 1U);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static char *hls_attr_value(const char *line, const char *key) {
    if (line == NULL || key == NULL) {
        return NULL;
    }
    const size_t key_len = strlen(key);
    const char *cursor = line;
    while ((cursor = strstr(cursor, key)) != NULL) {
        if (cursor[key_len] != '=') {
            cursor += key_len;
            continue;
        }
        cursor += key_len + 1U;
        if (*cursor == '"') {
            cursor++;
            const char *end = strchr(cursor, '"');
            if (end == NULL) {
                return NULL;
            }
            return dup_slice(cursor, (size_t)(end - cursor));
        }
        const char *end = cursor;
        while (*end != '\0' && *end != ',' && !isspace((unsigned char)*end)) {
            end++;
        }
        return dup_slice(cursor, (size_t)(end - cursor));
    }
    return NULL;
}

static void hls_playlist_free(HlsPlaylist *playlist) {
    if (playlist == NULL) {
        return;
    }
    for (size_t i = 0U; i < playlist->count; i++) {
        free(playlist->items[i].uri);
        free(playlist->items[i].key_uri);
    }
    free(playlist->items);
    playlist->items = NULL;
    playlist->count = 0U;
    playlist->capacity = 0U;
}

static int hls_playlist_push(HlsPlaylist *playlist, const HlsSegment *segment) {
    if (playlist == NULL || segment == NULL || segment->uri == NULL) {
        return -1;
    }
    if (playlist->count >= playlist->capacity) {
        const size_t next = playlist->capacity == 0U ? 32U : playlist->capacity * 2U;
        HlsSegment *items = realloc(playlist->items, next * sizeof(HlsSegment));
        if (items == NULL) {
            return -1;
        }
        playlist->items = items;
        playlist->capacity = next;
    }
    playlist->items[playlist->count] = *segment;
    playlist->items[playlist->count].uri = strdup(segment->uri);
    playlist->items[playlist->count].key_uri =
            segment->key_uri != NULL ? strdup(segment->key_uri) : NULL;
    if (playlist->items[playlist->count].uri == NULL) {
        return -1;
    }
    playlist->count++;
    return 0;
}

static bool playlist_is_master(const char *text) {
    return text != NULL && strstr(text, "#EXT-X-STREAM-INF") != NULL;
}

static char *pick_variant_url(const char *playlist, const char *base_url) {
    char *copy = strdup(playlist);
    if (copy == NULL) {
        return NULL;
    }

    char *best_uri = NULL;
    unsigned long best_bw = 0UL;
    unsigned long pending_bw = 0UL;
    bool have_inf = false;

    for (char *line = strtok(copy, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        line = trim_line(line);
        if (line[0] == '\0') {
            continue;
        }
        if (strncmp(line, "#EXT-X-STREAM-INF:", 18) == 0) {
            char *bw_text = hls_attr_value(line, "BANDWIDTH");
            pending_bw = bw_text != NULL ? strtoul(bw_text, NULL, 10) : 0UL;
            free(bw_text);
            have_inf = true;
            continue;
        }
        if (line[0] == '#' || !have_inf) {
            continue;
        }

        char *resolved = http_resolve_url(base_url, line, strlen(line));
        have_inf = false;
        if (resolved == NULL) {
            continue;
        }
        if (best_uri == NULL || pending_bw >= best_bw) {
            free(best_uri);
            best_uri = resolved;
            best_bw = pending_bw;
        } else {
            free(resolved);
        }
    }

    free(copy);
    return best_uri;
}

static int hls_parse_media_playlist(const char *playlist, const char *base_url,
                                    HlsPlaylist *out) {
    if (playlist == NULL || base_url == NULL || out == NULL) {
        return -1;
    }

    char *copy = strdup(playlist);
    if (copy == NULL) {
        return -1;
    }

    char *current_method = NULL;
    char *current_key_uri = NULL;
    unsigned char current_iv[16];
    bool current_iv_set = false;
    size_t media_sequence = 0U;
    bool have_sequence = false;
    size_t segment_index = 0U;
    int rc = 0;

    for (char *line = strtok(copy, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        line = trim_line(line);
        if (line[0] == '\0') {
            continue;
        }
        if (strncmp(line, "#EXT-X-MEDIA-SEQUENCE:", 22) == 0) {
            media_sequence = (size_t)strtoul(line + 22, NULL, 10);
            have_sequence = true;
            continue;
        }
        if (strncmp(line, "#EXT-X-KEY:", 11) == 0) {
            free(current_method);
            free(current_key_uri);
            current_method = hls_attr_value(line, "METHOD");
            current_key_uri = hls_attr_value(line, "URI");
            if (current_key_uri != NULL) {
                char *resolved = http_resolve_url(base_url, current_key_uri, strlen(current_key_uri));
                free(current_key_uri);
                current_key_uri = resolved;
            }
            char *iv_text = hls_attr_value(line, "IV");
            current_iv_set = iv_text != NULL && parse_hex_iv(iv_text, current_iv);
            free(iv_text);
            continue;
        }
        if (line[0] == '#') {
            continue;
        }

        char *segment_url = http_resolve_url(base_url, line, strlen(line));
        if (segment_url == NULL) {
            rc = -1;
            break;
        }

        HlsSegment segment = {
            .uri = segment_url,
            .key_uri = current_key_uri != NULL ? strdup(current_key_uri) : NULL,
            .iv_set = current_iv_set,
            .encrypted = current_method != NULL && str_case_equal(current_method, "AES-128"),
        };
        if (segment.iv_set) {
            memcpy(segment.iv, current_iv, sizeof segment.iv);
        } else if (segment.encrypted) {
            const size_t seq = have_sequence ? media_sequence + segment_index : segment_index;
            memset(segment.iv, 0, sizeof segment.iv);
            segment.iv[15] = (unsigned char)(seq & 0xFFU);
            segment.iv[14] = (unsigned char)((seq >> 8) & 0xFFU);
            segment.iv[13] = (unsigned char)((seq >> 16) & 0xFFU);
            segment.iv[12] = (unsigned char)((seq >> 24) & 0xFFU);
            segment.iv_set = true;
        }

        if (hls_playlist_push(out, &segment) != 0) {
            free(segment_url);
            free(segment.key_uri);
            rc = -1;
            break;
        }
        free(segment_url);
        free(segment.key_uri);
        segment_index++;
    }

    free(current_method);
    free(current_key_uri);
    free(copy);
    return rc;
}

static bool aes128_cbc_decrypt(const unsigned char *input, const size_t input_len,
                               const unsigned char key[16], const unsigned char iv[16],
                               unsigned char **output, size_t *output_len) {
    if (input == NULL || key == NULL || iv == NULL || output == NULL || output_len == NULL
        || input_len == 0U || (input_len % 16U) != 0U) {
        return false;
    }

    unsigned char *out = malloc(input_len);
    if (out == NULL) {
        return false;
    }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        free(out);
        return false;
    }

    unsigned char iv_copy[16];
    memcpy(iv_copy, iv, sizeof iv_copy);
    if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, input_len, iv_copy, input, out) != 0) {
        mbedtls_aes_free(&aes);
        free(out);
        return false;
    }
    mbedtls_aes_free(&aes);

    size_t len = input_len;
    if (len > 0U) {
        const unsigned char pad = out[len - 1U];
        if (pad > 0U && pad <= 16U && pad <= len) {
            len -= (size_t)pad;
        }
    }

    *output = out;
    *output_len = len;
    return true;
}

static bool hls_load_key(const char *key_uri, const char *referer, unsigned char key[16]) {
    size_t len = 0U;
    char *body = http_fetch_body(key_uri, referer, &len);
    if (body == NULL || len < 16U) {
        free(body);
        return false;
    }
    memcpy(key, body, 16U);
    free(body);
    return true;
}

static int hls_write_segment(FILE *fp, const HlsSegment *segment, const char *referer) {
    size_t len = 0U;
    unsigned char *body = (unsigned char *)http_fetch_body(segment->uri, referer, &len);
    if (body == NULL || len == 0U) {
        free(body);
        return -1;
    }

    unsigned char *plain = body;
    size_t plain_len = len;
    unsigned char *decrypted = NULL;

    if (segment->encrypted) {
        unsigned char key[16];
        if (segment->key_uri == NULL || !hls_load_key(segment->key_uri, referer, key)
            || !aes128_cbc_decrypt(body, len, key, segment->iv, &decrypted, &plain_len)) {
            free(body);
            return -1;
        }
        plain = decrypted;
    }

    const size_t written = fwrite(plain, 1, plain_len, fp);
    free(decrypted);
    free(body);
    return written == plain_len ? 0 : -1;
}

int stream_hls_download(const char *playlist_url, const char *dest_path, const char *referer) {
    if (playlist_url == NULL || dest_path == NULL) {
        return -1;
    }

    size_t playlist_len = 0U;
    char *playlist = http_fetch_body(playlist_url, referer, &playlist_len);
    if (playlist == NULL || playlist_len == 0U) {
        free(playlist);
        return -1;
    }

    const char *media_base = playlist_url;
    char *media_url = NULL;
    if (playlist_is_master(playlist)) {
        media_url = pick_variant_url(playlist, playlist_url);
        free(playlist);
        if (media_url == NULL) {
            return -1;
        }
        media_base = media_url;
        playlist = http_fetch_body(media_url, referer, &playlist_len);
        if (playlist == NULL) {
            free(media_url);
            return -1;
        }
    }

    HlsPlaylist segments = {0};
    if (hls_parse_media_playlist(playlist, media_base, &segments) != 0 || segments.count == 0U) {
        free(playlist);
        free(media_url);
        hls_playlist_free(&segments);
        return -1;
    }
    free(playlist);

    FILE *fp = fopen(dest_path, "wb");
    if (fp == NULL) {
        free(media_url);
        hls_playlist_free(&segments);
        return -1;
    }

    int rc = 0;
    for (size_t i = 0U; i < segments.count; i++) {
        if (hls_write_segment(fp, &segments.items[i], referer) != 0) {
            rc = -1;
            break;
        }
    }

    fclose(fp);
    free(media_url);
    hls_playlist_free(&segments);
    return rc;
}
