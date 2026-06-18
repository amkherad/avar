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

char *proxy_url_from_json(const void *proxy_obj) {
    const cJSON *proxy = (const cJSON *)proxy_obj;
    if (proxy == NULL || !cJSON_IsObject(proxy)) {
        return NULL;
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

    if (strcmp(type_str, AVAR_PROXY_TYPE_SOCKS5) == 0) {
        LOG_WARNING("SOCKS5 proxy is not supported yet; ignoring proxy settings");
        return NULL;
    }

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

void proxy_send_connect(struct mg_connection *c, const char *target_url) {
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
              "Proxy-Connection: keep-alive\r\n"
              "\r\n",
              hostport, hostport);
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
