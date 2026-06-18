#include "avar_test.h"
#include "test_guard.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <stdlib.h>
#else
    #include <unistd.h>
#endif

#include "avar.h"
#include "cJSON.h"
#include "config.h"
#include "http_proxy.h"

static TestGuard g_guard;

static void setup_temp_config(void) {
    AVAR_ASSERT(test_guard_init(&g_guard, "avar-test-http-proxy"));
    remove(g_guard.config_path);
    AVAR_ASSERT_EQ(config_open_at(g_guard.config_path), 0);
}

static void set_env_var(const char *name, const char *value) {
#if defined(_WIN32)
    char entry[AVAR_CONFIG_PATH_MAX];
    snprintf(entry, sizeof entry, "%s=%s", name, value);
    AVAR_ASSERT_EQ(_putenv(entry), 0);
#else
    AVAR_ASSERT_EQ(setenv(name, value, 1), 0);
#endif
}

static void unset_env_var(const char *name) {
#if defined(_WIN32)
    char entry[AVAR_CONFIG_PATH_MAX];
    snprintf(entry, sizeof entry, "%s=", name);
    AVAR_ASSERT_EQ(_putenv(entry), 0);
#else
    AVAR_ASSERT_EQ(unsetenv(name), 0);
#endif
}

AVAR_TEST(proxy_build_url_with_credentials) {
    char *url = proxy_build_url("http", "proxy.example.com", 8080U, "user", "secret");
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "http://user:secret@proxy.example.com:8080");
    free(url);
}

AVAR_TEST(proxy_normalize_url_adds_scheme) {
    char *url = proxy_normalize_url("127.0.0.1:3128");
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "http://127.0.0.1:3128");
    free(url);
}

AVAR_TEST(proxy_kind_detects_socks5) {
    AVAR_ASSERT(proxy_kind_from_url("socks5://127.0.0.1:1080") == ProxyKindSocks5);
    AVAR_ASSERT(proxy_kind_from_url("http://127.0.0.1:8080") == ProxyKindHttp);
    AVAR_ASSERT(proxy_kind_from_url("https://127.0.0.1:8443") == ProxyKindHttps);
}

AVAR_TEST(proxy_connect_url_strips_credentials) {
    char *url = proxy_connect_url("http://user:pass@127.0.0.1:8080");
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "http://127.0.0.1:8080");
    free(url);

    url = proxy_connect_url("socks5://127.0.0.1:1080");
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "tcp://127.0.0.1:1080");
    free(url);
}

AVAR_TEST(proxy_connect_response_ok_accepts_200) {
    AVAR_ASSERT(proxy_connect_response_ok("HTTP/1.1 200 Connection established\r\n\r\n", 39));
    AVAR_ASSERT(!proxy_connect_response_ok("HTTP/1.1 403 Forbidden\r\n\r\n", 26));
}

AVAR_TEST(proxy_should_bypass_localhost) {
    AVAR_ASSERT(proxy_should_bypass("http://localhost/file", "localhost,127.0.0.1"));
    AVAR_ASSERT(proxy_should_bypass("https://127.0.0.1/file", "localhost,127.0.0.1"));
    AVAR_ASSERT(!proxy_should_bypass("https://example.com/file", "localhost,127.0.0.1"));
    AVAR_ASSERT(proxy_should_bypass("https://cdn.example.com/file", "*.example.com"));
}

AVAR_TEST(proxy_load_global_url_from_config) {
    setup_temp_config();

    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_PROXY_ENABLED, "true"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_PROXY_TYPE, "http"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_PROXY_HOST, "10.0.0.5"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_PROXY_PORT, "8080"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_PROXY_USERNAME, "alice"), 0);
    AVAR_ASSERT_EQ(set_config(AVAR_CFG_DM_PROXY_PASSWORD, "pw"), 0);

    char *url = proxy_load_global_url();
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "http://alice:pw@10.0.0.5:8080");
    free(url);
}

AVAR_TEST(proxy_load_from_environment_https) {
    setup_temp_config();
    unset_env_var("HTTP_PROXY");
    unset_env_var("HTTPS_PROXY");

    set_env_var("HTTPS_PROXY", "https://secure-proxy:8443");

    char *url = proxy_load_from_environment("https://example.com/file.bin");
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "https://secure-proxy:8443");
    free(url);

    unset_env_var("HTTPS_PROXY");
}

AVAR_TEST(proxy_resolve_honors_no_proxy) {
    setup_temp_config();
    set_env_var("HTTP_PROXY", "http://proxy:8080");
    set_env_var("NO_PROXY", "example.com");

    char *url = proxy_resolve_for_target("http://example.com/a.bin", NULL);
    AVAR_ASSERT_NULL(url);

    url = proxy_resolve_for_target("http://other.example/a.bin", NULL);
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "http://proxy:8080");
    free(url);

    unset_env_var("HTTP_PROXY");
    unset_env_var("NO_PROXY");
}

AVAR_TEST(proxy_url_from_json_object) {
    const char *json =
            "{\"type\":\"socks5\",\"host\":\"127.0.0.1\",\"port\":1080,\"username\":\"u\","
            "\"password\":\"p\"}";
    cJSON *root = cJSON_Parse(json);
    AVAR_ASSERT_NOT_NULL(root);

    char *url = proxy_url_from_json(root);
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "socks5://u:p@127.0.0.1:1080");
    free(url);
    cJSON_Delete(root);
}

AVAR_TEST(proxy_url_from_json_string) {
    cJSON *value = cJSON_CreateString("http://127.0.0.1:8080");
    AVAR_ASSERT_NOT_NULL(value);

    char *url = proxy_url_from_json(value);
    AVAR_ASSERT_NOT_NULL(url);
    AVAR_ASSERT_STR_EQ(url, "http://127.0.0.1:8080");
    free(url);
    cJSON_Delete(value);
}

AVAR_TEST(proxy_uses_http_forward_only_for_http_targets) {
    AVAR_ASSERT(proxy_uses_http_forward("http://127.0.0.1:8080", "http://example.com/a"));
    AVAR_ASSERT(!proxy_uses_http_forward("http://127.0.0.1:8080", "https://example.com/a"));
    AVAR_ASSERT(!proxy_uses_http_forward("socks5://127.0.0.1:1080", "http://example.com/a"));
}

AVAR_TEST_MAIN(
        run_proxy_build_url_with_credentials();
        run_proxy_normalize_url_adds_scheme();
        run_proxy_kind_detects_socks5();
        run_proxy_connect_url_strips_credentials();
        run_proxy_connect_response_ok_accepts_200();
        run_proxy_should_bypass_localhost();
        run_proxy_load_global_url_from_config();
        run_proxy_load_from_environment_https();
        run_proxy_resolve_honors_no_proxy();
        run_proxy_url_from_json_object();
        run_proxy_url_from_json_string();
        run_proxy_uses_http_forward_only_for_http_targets();)
