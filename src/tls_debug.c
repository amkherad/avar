#include <tls_debug.h>

#include <logger.h>
#include <mongoose.h>

#include <stdio.h>
#include <string.h>

#if MG_TLS == MG_TLS_MBED
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>

void tls_apply_client_policy(struct mg_connection *c) {
    if (c == NULL || c->tls == NULL || c->is_listening) {
        return;
    }

    struct mg_tls *tls = (struct mg_tls *)c->tls;

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    mbedtls_ssl_conf_min_tls_version(&tls->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&tls->conf, MBEDTLS_SSL_VERSION_TLS1_3);
#else
    mbedtls_ssl_conf_min_version(&tls->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&tls->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
#endif
}

static const char *tls_alert_name(const unsigned char code) {
    switch (code) {
        case MBEDTLS_SSL_ALERT_MSG_CLOSE_NOTIFY:
            return "close_notify";
        case MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE:
            return "unexpected_message";
        case MBEDTLS_SSL_ALERT_MSG_BAD_RECORD_MAC:
            return "bad_record_mac";
        case MBEDTLS_SSL_ALERT_MSG_DECRYPTION_FAILED:
            return "decryption_failed";
        case MBEDTLS_SSL_ALERT_MSG_RECORD_OVERFLOW:
            return "record_overflow";
        case MBEDTLS_SSL_ALERT_MSG_DECOMPRESSION_FAILURE:
            return "decompression_failure";
        case MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE:
            return "handshake_failure";
        case MBEDTLS_SSL_ALERT_MSG_NO_CERT:
            return "no_cert";
        case MBEDTLS_SSL_ALERT_MSG_BAD_CERT:
            return "bad_cert";
        case MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_CERT:
            return "unsupported_cert";
        case MBEDTLS_SSL_ALERT_MSG_CERT_REVOKED:
            return "cert_revoked";
        case MBEDTLS_SSL_ALERT_MSG_CERT_EXPIRED:
            return "cert_expired";
        case MBEDTLS_SSL_ALERT_MSG_CERT_UNKNOWN:
            return "cert_unknown";
        case MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER:
            return "illegal_parameter";
        case MBEDTLS_SSL_ALERT_MSG_UNKNOWN_CA:
            return "unknown_ca";
        case MBEDTLS_SSL_ALERT_MSG_ACCESS_DENIED:
            return "access_denied";
        case MBEDTLS_SSL_ALERT_MSG_DECODE_ERROR:
            return "decode_error";
        case MBEDTLS_SSL_ALERT_MSG_DECRYPT_ERROR:
            return "decrypt_error";
        case MBEDTLS_SSL_ALERT_MSG_EXPORT_RESTRICTION:
            return "export_restriction";
        case MBEDTLS_SSL_ALERT_MSG_PROTOCOL_VERSION:
            return "protocol_version";
        case MBEDTLS_SSL_ALERT_MSG_INSUFFICIENT_SECURITY:
            return "insufficient_security";
        case MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR:
            return "internal_error";
        case MBEDTLS_SSL_ALERT_MSG_INAPROPRIATE_FALLBACK:
            return "inappropriate_fallback";
        case MBEDTLS_SSL_ALERT_MSG_USER_CANCELED:
            return "user_canceled";
        case MBEDTLS_SSL_ALERT_MSG_NO_RENEGOTIATION:
            return "no_renegotiation";
        case MBEDTLS_SSL_ALERT_MSG_UNSUPPORTED_EXT:
            return "unsupported_extension";
        case MBEDTLS_SSL_ALERT_MSG_UNRECOGNIZED_NAME:
            return "unrecognized_name";
        case MBEDTLS_SSL_ALERT_MSG_UNKNOWN_PSK_IDENTITY:
            return "unknown_psk_identity";
        case MBEDTLS_SSL_ALERT_MSG_NO_APPLICATION_PROTOCOL:
            return "no_application_protocol";
        default:
            return "unknown";
    }
}

static void log_mbedtls_details(struct mg_connection *c, const char *message) {
    if (c == NULL || c->tls == NULL) {
        return;
    }

    const struct mg_tls *tls = (const struct mg_tls *)c->tls;
    const mbedtls_ssl_context *ssl = &tls->ssl;

#if MBEDTLS_VERSION_NUMBER < 0x03000000
    if (ssl->in_msgtype == MBEDTLS_SSL_MSG_ALERT && ssl->in_msglen >= 2U && ssl->in_msg != NULL) {
        const unsigned char level = ssl->in_msg[0];
        const unsigned char description = ssl->in_msg[1];
        LOG_ERROR("TLS peer alert: level=%u (%s) description=%u (%s)", level,
                  level == MBEDTLS_SSL_ALERT_LEVEL_FATAL ? "fatal" : "warning", description,
                  tls_alert_name(description));
        if (description == MBEDTLS_SSL_ALERT_MSG_PROTOCOL_VERSION) {
            LOG_ERROR("TLS hint: server rejected the client protocol version (TLS 1.3 may be required)");
        }
    }
#endif

    const char *version = mbedtls_ssl_get_version(ssl);
    if (version != NULL && version[0] != '\0') {
        LOG_ERROR("TLS version: %s", version);
    }

    const char *ciphersuite = mbedtls_ssl_get_ciphersuite(ssl);
    if (ciphersuite != NULL && ciphersuite[0] != '\0') {
        LOG_ERROR("TLS ciphersuite: %s", ciphersuite);
    }

    if (message != NULL) {
        unsigned int code = 0U;
        if (sscanf(message, "TLS handshake: -%x", &code) == 1 ||
            sscanf(message, "TLS handshake: -0x%x", &code) == 1) {
            char errbuf[128];
            mbedtls_strerror(-(int)code, errbuf, sizeof errbuf);
            LOG_ERROR("TLS mbedTLS error -0x%x: %s", code, errbuf);
            if (code == 0x7780U) {
                LOG_ERROR("TLS hint: peer sent a fatal alert during handshake (often protocol_version or "
                          "insufficient_security)");
            }
        }
    }
}
#else
void tls_apply_client_policy(struct mg_connection *c) {
    (void)c;
}

static void log_mbedtls_details(struct mg_connection *c, const char *message) {
    (void)c;
    (void)message;
}
#endif

void tls_log_handshake_start(const char *url, const char *sni_host) {
    if (url == NULL || url[0] == '\0') {
        return;
    }

    if (sni_host != NULL && sni_host[0] != '\0') {
        LOG_INFO("TLS handshake starting: url=%s sni=%s range=TLSv1.2-TLSv1.3", url, sni_host);
    } else {
        LOG_INFO("TLS handshake starting: url=%s range=TLSv1.2-TLSv1.3", url);
    }
}

void tls_log_download_error(struct mg_connection *c, const char *url, const char *item_id,
                            const char *proxy_url, const uint64_t range_start,
                            const uint64_t range_end, const bool has_range, const char *message) {
    const char *safe_url = (url != NULL && url[0] != '\0') ? url : "(unknown)";
    const char *safe_message = (message != NULL && message[0] != '\0') ? message : "(none)";

    if (item_id != NULL && item_id[0] != '\0') {
        LOG_ERROR("Download connection error: job=%s url=%s error=%s", item_id, safe_url, safe_message);
    } else {
        LOG_ERROR("Download connection error: url=%s error=%s", safe_url, safe_message);
    }

    if (proxy_url != NULL && proxy_url[0] != '\0') {
        LOG_ERROR("Download connection proxy: %s", proxy_url);
    }

    if (has_range) {
        LOG_ERROR("Download connection segment: %llu-%llu", (unsigned long long)range_start,
                  (unsigned long long)range_end);
    }

    if (c != NULL && c->is_tls) {
        log_mbedtls_details(c, message);
    }
}
