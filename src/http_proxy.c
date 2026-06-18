#include <http_proxy.h>
#include <avar.h>

#include <cJSON.h>
#include <mongoose.h>

#include <config.h>
#include <logger.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
static int ascii_stricmp(const char *a, const char *b) {
    return _stricmp(a, b);
}
#else
#include <strings.h>
static int ascii_stricmp(const char *a, const char *b) {
    return strcasecmp(a, b);
}
#endif

static bool parse_config_bool_local(const char *key, const bool default_value) {
    char *value = get_config_or_default(key, NULL);
    if (value == NULL) {
        return default_value;
    }

    const bool result =
            strcmp(value, "1") == 0 || ascii_stricmp(value, "true") == 0 || ascii_stricmp(value, "yes") == 0;
    free(value);
    return result;
}

static char *url_encode_credentials(const char *value) {
    if (value == NULL || value[0] == '\0') {
        return NULL;
    }

    char buf[512];
    const size_t n = mg_url_encode(value, strlen(value), buf, sizeof buf);
    if (n == 0) {
        return strdup(value);
    }
    return strndup(buf, n);
}

static const char *env_get_any(const char *const *names) {
    for (size_t i = 0; names[i] != NULL; i++) {
        const char *value = getenv(names[i]);
        if (value != NULL && value[0] != '\0') {
            return value;
        }
    }
    return NULL;
}

static void trim_entry(char *entry) {
    if (entry == NULL) {
        return;
    }

    char *start = entry;
    while (*start == ' ' || *start == '\t') {
        start++;
    }

    size_t len = strlen(start);
    while (len > 0U && (start[len - 1U] == ' ' || start[len - 1U] == '\t')) {
        len--;
    }
    start[len] = '\0';

    if (start != entry) {
        memmove(entry, start, len + 1U);
    }
}

static bool host_matches_no_proxy_entry(const char *host, unsigned port, const char *entry) {
    if (host == NULL || entry == NULL || entry[0] == '\0') {
        return false;
    }

    char pattern[320];
    strncpy(pattern, entry, sizeof pattern - 1U);
    pattern[sizeof pattern - 1U] = '\0';
    trim_entry(pattern);
    if (pattern[0] == '\0') {
        return false;
    }

    unsigned entry_port = 0U;
    char *colon = strrchr(pattern, ':');
    if (colon != NULL && strchr(colon + 1, ':') == NULL) {
        char *end = NULL;
        const unsigned parsed = (unsigned)strtoul(colon + 1, &end, 10);
        if (end != colon + 1 && *end == '\0') {
            entry_port = parsed;
            *colon = '\0';
        }
    }

    if (entry_port != 0U && port != 0U && entry_port != port) {
        return false;
    }

    if (strcmp(pattern, "*") == 0) {
        return true;
    }

    if (ascii_stricmp(host, pattern) == 0) {
        return true;
    }

    const size_t pattern_len = strlen(pattern);
    if (pattern_len > 1U && pattern[0] == '.' && ascii_stricmp(host, pattern + 1) == 0) {
        return true;
    }

    if (pattern_len > 2U && pattern[0] == '*' && pattern[1] == '.') {
        const char *suffix = pattern + 1;
        const size_t host_len = strlen(host);
        const size_t suffix_len = strlen(suffix);
        if (host_len >= suffix_len && ascii_stricmp(host + host_len - suffix_len, suffix) == 0) {
            return true;
        }
    }

    if (pattern_len > 0U && pattern[0] != '.' && pattern[0] != '*') {
        const size_t host_len = strlen(host);
        if (host_len > pattern_len && host[host_len - pattern_len - 1U] == '.'
            && ascii_stricmp(host + host_len - pattern_len, pattern) == 0) {
            return true;
        }
    }

    return false;
}

static bool extract_proxy_credentials(const char *proxy_url, char *user_out, size_t user_cap,
                                      char *pass_out, size_t pass_cap) {
    if (proxy_url == NULL || user_out == NULL || pass_out == NULL || user_cap == 0U || pass_cap == 0U) {
        return false;
    }

    user_out[0] = '\0';
    pass_out[0] = '\0';

    const char *scheme_end = strstr(proxy_url, "://");
    const char *auth_start = scheme_end != NULL ? scheme_end + 3U : proxy_url;
    const char *at = strchr(auth_start, '@');
    if (at == NULL) {
        return false;
    }

    const size_t auth_len = (size_t)(at - auth_start);
    if (auth_len == 0U || auth_len >= 512U) {
        return false;
    }

    char auth_buf[512];
    memcpy(auth_buf, auth_start, auth_len);
    auth_buf[auth_len] = '\0';

    char *colon = strchr(auth_buf, ':');
    if (colon == NULL) {
        strncpy(user_out, auth_buf, user_cap - 1U);
        user_out[user_cap - 1U] = '\0';
        return user_out[0] != '\0';
    }

    *colon = '\0';
    strncpy(user_out, auth_buf, user_cap - 1U);
    user_out[user_cap - 1U] = '\0';
    strncpy(pass_out, colon + 1, pass_cap - 1U);
    pass_out[pass_cap - 1U] = '\0';
    return user_out[0] != '\0';
}

static void base64_encode_triplet(const unsigned char in[3], size_t len, char out[4]) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    out[0] = table[(in[0] >> 2) & 0x3F];
    out[1] = table[((in[0] & 0x03) << 4) | ((in[1] >> 4) & 0x0F)];
    out[2] = len > 1U ? table[((in[1] & 0x0F) << 2) | ((in[2] >> 6) & 0x03)] : '=';
    out[3] = len > 2U ? table[in[2] & 0x3F] : '=';
}

static char *base64_encode(const char *input) {
    if (input == NULL) {
        return NULL;
    }

    const size_t in_len = strlen(input);
    const size_t out_len = 4U * ((in_len + 2U) / 3U) + 1U;
    char *out = malloc(out_len);
    if (out == NULL) {
        return NULL;
    }

    size_t out_pos = 0U;
    for (size_t i = 0U; i < in_len; i += 3U) {
        unsigned char block[3] = {0, 0, 0};
        const size_t block_len = in_len - i >= 3U ? 3U : in_len - i;
        memcpy(block, input + i, block_len);
        char chunk[4];
        base64_encode_triplet(block, block_len, chunk);
        memcpy(out + out_pos, chunk, 4U);
        out_pos += 4U;
    }
    out[out_pos] = '\0';
    return out;
}

static void append_proxy_authorization(struct mg_connection *c, const char *proxy_url) {
    char user[256];
    char pass[256];
    if (!extract_proxy_credentials(proxy_url, user, sizeof user, pass, sizeof pass)) {
        return;
    }

    char creds[512];
    snprintf(creds, sizeof creds, "%s:%s", user, pass);
    char *encoded = base64_encode(creds);
    if (encoded == NULL) {
        return;
    }

    mg_printf(c, "Proxy-Authorization: Basic %s\r\n", encoded);
    free(encoded);
}

ProxyKind proxy_kind_from_url(const char *proxy_url) {
    if (proxy_url == NULL || proxy_url[0] == '\0') {
        return ProxyKindNone;
    }

    if (strncmp(proxy_url, "socks5://", 9) == 0 || strncmp(proxy_url, "socks5h://", 10) == 0) {
        return ProxyKindSocks5;
    }
    if (strncmp(proxy_url, "https://", 8) == 0 || strncmp(proxy_url, "HTTPS://", 8) == 0) {
        return ProxyKindHttps;
    }
    if (strncmp(proxy_url, "http://", 7) == 0 || strncmp(proxy_url, "HTTP://", 7) == 0) {
        return ProxyKindHttp;
    }

    return ProxyKindHttp;
}

char *proxy_normalize_url(const char *url) {
    if (url == NULL || url[0] == '\0') {
        return NULL;
    }

    if (strstr(url, "://") != NULL) {
        return strdup(url);
    }

    const size_t cap = strlen(url) + 16U;
    char *normalized = malloc(cap);
    if (normalized == NULL) {
        return NULL;
    }
    snprintf(normalized, cap, "http://%s", url);
    return normalized;
}

char *proxy_connect_url(const char *proxy_url) {
    if (proxy_url == NULL || proxy_url[0] == '\0') {
        return NULL;
    }

    const ProxyKind kind = proxy_kind_from_url(proxy_url);
    if (kind == ProxyKindSocks5) {
        struct mg_str host = mg_url_host(proxy_url);
        const unsigned port = mg_url_port(proxy_url);
        if (host.len == 0U) {
            return NULL;
        }

        char *connect = malloc(host.len + 32U);
        if (connect == NULL) {
            return NULL;
        }

        if (port != 0U) {
            snprintf(connect, host.len + 32U, "tcp://%.*s:%u", (int)host.len, host.buf, port);
        } else {
            snprintf(connect, host.len + 32U, "tcp://%.*s:1080", (int)host.len, host.buf);
        }
        return connect;
    }

    if (kind == ProxyKindHttps) {
        return strdup(proxy_url);
    }

    struct mg_str host = mg_url_host(proxy_url);
    const unsigned port = mg_url_port(proxy_url);
    if (host.len == 0U) {
        return NULL;
    }

    char *connect = malloc(host.len + 32U);
    if (connect == NULL) {
        return NULL;
    }

    if (port != 0U) {
        snprintf(connect, host.len + 32U, "http://%.*s:%u", (int)host.len, host.buf, port);
    } else {
        snprintf(connect, host.len + 32U, "http://%.*s:8080", (int)host.len, host.buf);
    }
    return connect;
}

char *proxy_build_url(const char *type, const char *host, const unsigned port,
                      const char *username, const char *password) {
    if (host == NULL || host[0] == '\0' || port == 0U) {
        return NULL;
    }

    const char *scheme = AVAR_PROXY_TYPE_HTTP;
    if (type != NULL && type[0] != '\0') {
        scheme = type;
    }

    char *user = url_encode_credentials(username);
    char *pass = url_encode_credentials(password);

    size_t cap = strlen(scheme) + strlen(host) + 32U;
    if (user != NULL) {
        cap += strlen(user) + 1U;
    }
    if (pass != NULL) {
        cap += strlen(pass) + 1U;
    }

    char *url = malloc(cap);
    if (url == NULL) {
        free(user);
        free(pass);
        return NULL;
    }

    if (user != NULL && pass != NULL) {
        snprintf(url, cap, "%s://%s:%s@%s:%u", scheme, user, pass, host, port);
    } else if (user != NULL) {
        snprintf(url, cap, "%s://%s@%s:%u", scheme, user, host, port);
    } else {
        snprintf(url, cap, "%s://%s:%u", scheme, host, port);
    }

    free(user);
    free(pass);
    return url;
}

char *proxy_get_no_proxy_list(void) {
    static const char *const names[] = {"NO_PROXY", "no_proxy", NULL};
    const char *value = env_get_any(names);
    if (value != NULL) {
        return strdup(value);
    }

    return get_config_or_default(AVAR_CFG_DM_PROXY_NO_PROXY, NULL);
}

bool proxy_should_bypass(const char *target_url, const char *no_proxy_list) {
    if (target_url == NULL || no_proxy_list == NULL || no_proxy_list[0] == '\0') {
        return false;
    }

    struct mg_str host = mg_url_host(target_url);
    if (host.len == 0U || host.buf == NULL) {
        return false;
    }

    char host_buf[320];
    const size_t copy = host.len < sizeof host_buf - 1U ? host.len : sizeof host_buf - 1U;
    memcpy(host_buf, host.buf, copy);
    host_buf[copy] = '\0';

    const unsigned port = mg_url_port(target_url);

    char *list = strdup(no_proxy_list);
    if (list == NULL) {
        return false;
    }

    bool bypass = false;
    char *cursor = list;
    while (cursor != NULL && *cursor != '\0') {
        char *comma = strchr(cursor, ',');
        if (comma != NULL) {
            *comma = '\0';
        }

        if (host_matches_no_proxy_entry(host_buf, port, cursor)) {
            bypass = true;
            break;
        }

        cursor = comma != NULL ? comma + 1 : NULL;
    }

    free(list);
    return bypass;
}

char *proxy_load_from_environment(const char *target_url) {
    static const char *const https_names[] = {"HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy",
                                              "ALL_PROXY",   "all_proxy",   NULL};
    static const char *const http_names[] = {"HTTP_PROXY", "http_proxy", "ALL_PROXY", "all_proxy", NULL};

    const char *const *names = proxy_target_is_https(target_url) ? https_names : http_names;
    const char *value = env_get_any(names);
    if (value == NULL) {
        return NULL;
    }

    return proxy_normalize_url(value);
}

char *proxy_load_global_url(void) {
    if (!parse_config_bool_local(AVAR_CFG_DM_PROXY_ENABLED, false)) {
        return NULL;
    }

    char *host = get_config_or_default(AVAR_CFG_DM_PROXY_HOST, NULL);
    char *port_str = get_config_or_default(AVAR_CFG_DM_PROXY_PORT, NULL);
    char *type = get_config_or_default(AVAR_CFG_DM_PROXY_TYPE, AVAR_PROXY_TYPE_HTTP);
    char *username = get_config_or_default(AVAR_CFG_DM_PROXY_USERNAME, NULL);
    char *password = get_config_or_default(AVAR_CFG_DM_PROXY_PASSWORD, NULL);

    unsigned port = 0U;
    if (port_str != NULL) {
        port = (unsigned)strtoul(port_str, NULL, 10);
    }

    char *url = NULL;
    if (host != NULL && host[0] != '\0' && port > 0U) {
        url = proxy_build_url(type, host, port, username, password);
    }

    free(host);
    free(port_str);
    free(type);
    free(username);
    free(password);
    return url;
}

char *proxy_resolve_for_target(const char *target_url, const char *override_url) {
    if (target_url == NULL) {
        return NULL;
    }

    char *no_proxy = proxy_get_no_proxy_list();
    char *resolved = NULL;

    if (override_url != NULL && override_url[0] != '\0') {
        resolved = proxy_normalize_url(override_url);
    } else {
        resolved = proxy_load_from_environment(target_url);
        if (resolved == NULL) {
            resolved = proxy_load_global_url();
        }
    }

    if (resolved != NULL && proxy_should_bypass(target_url, no_proxy)) {
        free(resolved);
        resolved = NULL;
    }

    free(no_proxy);
    return resolved;
}

char *proxy_url_from_json(const void *proxy_obj) {
    const cJSON *proxy = (const cJSON *)proxy_obj;
    if (proxy == NULL) {
        return NULL;
    }

    if (cJSON_IsString(proxy) && proxy->valuestring != NULL) {
        return proxy_normalize_url(proxy->valuestring);
    }

    if (!cJSON_IsObject(proxy)) {
        return NULL;
    }

    const cJSON *url_json = cJSON_GetObjectItemCaseSensitive(proxy, "url");
    if (cJSON_IsString(url_json) && url_json->valuestring != NULL && url_json->valuestring[0] != '\0') {
        return proxy_normalize_url(url_json->valuestring);
    }

    const cJSON *host = cJSON_GetObjectItemCaseSensitive(proxy, "host");
    if (!cJSON_IsString(host) || host->valuestring == NULL || host->valuestring[0] == '\0') {
        return NULL;
    }

    unsigned port = 8080U;
    const cJSON *port_json = cJSON_GetObjectItemCaseSensitive(proxy, "port");
    if (cJSON_IsNumber(port_json) && port_json->valuedouble > 0) {
        port = (unsigned)port_json->valuedouble;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(proxy, "type");
    const char *type_str = cJSON_IsString(type) && type->valuestring != NULL ? type->valuestring
                                                                               : AVAR_PROXY_TYPE_HTTP;

    const cJSON *username = cJSON_GetObjectItemCaseSensitive(proxy, "username");
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(proxy, "password");

    return proxy_build_url(type_str, host->valuestring, port,
                           cJSON_IsString(username) ? username->valuestring : NULL,
                           cJSON_IsString(password) ? password->valuestring : NULL);
}

bool proxy_target_is_https(const char *url) {
    if (url == NULL) {
        return false;
    }
    return strncmp(url, "https://", 8) == 0 || strncmp(url, "HTTPS://", 8) == 0;
}

bool proxy_uses_http_forward(const char *proxy_url, const char *target_url) {
    if (proxy_url == NULL || target_url == NULL) {
        return false;
    }

    const ProxyKind kind = proxy_kind_from_url(proxy_url);
    if (kind == ProxyKindSocks5) {
        return false;
    }

    return !proxy_target_is_https(target_url);
}

void proxy_send_connect(struct mg_connection *c, const char *target_url, const char *proxy_url) {
    if (c == NULL || target_url == NULL) {
        return;
    }

    struct mg_str host = mg_url_host(target_url);
    const unsigned port_num = mg_url_port(target_url);
    if (host.len == 0) {
        return;
    }

    char hostport[320];
    if (port_num != 0U) {
        snprintf(hostport, sizeof hostport, "%.*s:%u", (int)host.len, host.buf, port_num);
    } else {
        const char *default_port = proxy_target_is_https(target_url) ? "443" : "80";
        snprintf(hostport, sizeof hostport, "%.*s:%s", (int)host.len, host.buf, default_port);
    }

    mg_printf(c,
              "CONNECT %s HTTP/1.1\r\n"
              "Host: %s\r\n"
              "Proxy-Connection: keep-alive\r\n",
              hostport, hostport);
    append_proxy_authorization(c, proxy_url);
    mg_printf(c, "\r\n");
}

bool proxy_connect_response_ok(const char *buf, const size_t len) {
    if (buf == NULL || len < 12U) {
        return false;
    }

    char line[128];
    const size_t copy = len < sizeof line - 1U ? len : sizeof line - 1U;
    memcpy(line, buf, copy);
    line[copy] = '\0';

    return strstr(line, " 200") != NULL || strstr(line, " 201") != NULL;
}

static void proxy_socks5_send_greeting(struct mg_connection *c, const ProxySocksCtx *ctx) {
    if (ctx->has_auth) {
        const unsigned char greeting[] = {0x05, 0x02, 0x00, 0x02};
        mg_send(c, greeting, sizeof greeting);
    } else {
        const unsigned char greeting[] = {0x05, 0x01, 0x00};
        mg_send(c, greeting, sizeof greeting);
    }
}

static void proxy_socks5_send_auth(struct mg_connection *c, const ProxySocksCtx *ctx) {
    const size_t ulen = strlen(ctx->username);
    const size_t plen = strlen(ctx->password);
    if (ulen > 255U || plen > 255U) {
        return;
    }

    unsigned char req[3U + 255U + 255U];
    size_t pos = 0U;
    req[pos++] = 0x01;
    req[pos++] = (unsigned char)ulen;
    memcpy(req + pos, ctx->username, ulen);
    pos += ulen;
    req[pos++] = (unsigned char)plen;
    memcpy(req + pos, ctx->password, plen);
    pos += plen;
    mg_send(c, req, pos);
}

static void proxy_socks5_send_connect(struct mg_connection *c, const char *target_url) {
    struct mg_str host = mg_url_host(target_url);
    if (host.len == 0U || host.len > 255U) {
        return;
    }

    unsigned port = mg_url_port(target_url);
    if (port == 0U) {
        port = proxy_target_is_https(target_url) ? 443U : 80U;
    }

    unsigned char req[4U + 1U + 255U + 2U];
    size_t pos = 0U;
    req[pos++] = 0x05;
    req[pos++] = 0x01;
    req[pos++] = 0x00;
    req[pos++] = 0x03;
    req[pos++] = (unsigned char)host.len;
    memcpy(req + pos, host.buf, host.len);
    pos += host.len;
    req[pos++] = (unsigned char)((port >> 8) & 0xFF);
    req[pos++] = (unsigned char)(port & 0xFF);
    mg_send(c, req, pos);
}

void proxy_socks5_begin(struct mg_connection *c, const char *target_url, const char *proxy_url,
                        ProxySocksCtx *ctx) {
    if (c == NULL || target_url == NULL || ctx == NULL) {
        return;
    }

    ctx->stage = ProxySocksStageNone;
    ctx->has_auth = false;
    ctx->username[0] = '\0';
    ctx->password[0] = '\0';

    if (extract_proxy_credentials(proxy_url, ctx->username, sizeof ctx->username, ctx->password,
                                  sizeof ctx->password)) {
        ctx->has_auth = true;
    }

    proxy_socks5_send_greeting(c, ctx);
    ctx->stage = ProxySocksStageGreetingSent;
}

bool proxy_socks5_handle_read(const char *buf, const size_t len, struct mg_connection *c,
                              const char *target_url, const char *proxy_url, ProxySocksCtx *ctx) {
    (void)proxy_url;

    if (buf == NULL || c == NULL || target_url == NULL || ctx == NULL) {
        return false;
    }

    if (ctx->stage == ProxySocksStageGreetingSent) {
        if (len < 2U || (unsigned char)buf[0] != 0x05) {
            ctx->stage = ProxySocksStageFailed;
            return false;
        }

        const unsigned char method = (unsigned char)buf[1];
        if (method == 0xFF) {
            ctx->stage = ProxySocksStageFailed;
            return false;
        }

        if (method == 0x02) {
            if (!ctx->has_auth) {
                ctx->stage = ProxySocksStageFailed;
                return false;
            }
            proxy_socks5_send_auth(c, ctx);
            ctx->stage = ProxySocksStageAuthSent;
            return false;
        }

        if (method != 0x00) {
            ctx->stage = ProxySocksStageFailed;
            return false;
        }

        proxy_socks5_send_connect(c, target_url);
        ctx->stage = ProxySocksStageConnectSent;
        return false;
    }

    if (ctx->stage == ProxySocksStageAuthSent) {
        if (len < 2U || (unsigned char)buf[0] != 0x01 || (unsigned char)buf[1] != 0x00) {
            ctx->stage = ProxySocksStageFailed;
            return false;
        }

        proxy_socks5_send_connect(c, target_url);
        ctx->stage = ProxySocksStageConnectSent;
        return false;
    }

    if (ctx->stage == ProxySocksStageConnectSent) {
        if (len < 4U || (unsigned char)buf[0] != 0x05 || (unsigned char)buf[1] != 0x00) {
            ctx->stage = ProxySocksStageFailed;
            return false;
        }

        ctx->stage = ProxySocksStageDone;
        return true;
    }

    return ctx->stage == ProxySocksStageDone;
}
