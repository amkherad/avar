#include "avar/extension_protocol.h"

#include <stdio.h>
#include <string.h>

static const char *find_json_string(const char *body, const char *key)
{
    if (body == NULL || key == NULL) {
        return NULL;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(body, pattern);
    if (pos == NULL) {
        return NULL;
    }
    pos = strchr(pos + strlen(pattern), '"');
    if (pos == NULL) {
        return NULL;
    }
    return pos + 1;
}

bool avar_extension_parse_envelope(const char *body, avar_extension_message *out, char *error, size_t error_size)
{
    if (out == NULL || body == NULL) {
        if (error != NULL && error_size > 0) {
            snprintf(error, error_size, "Invalid arguments");
        }
        return false;
    }

    if (strstr(body, "\"protocol\":\"" AVAR_EXTENSION_PROTOCOL_NAME "\"") == NULL &&
        strstr(body, "\"protocol\": \"" AVAR_EXTENSION_PROTOCOL_NAME "\"") == NULL) {
        snprintf(error, error_size, "Unknown protocol");
        return false;
    }

    const char *version = strstr(body, "\"version\":");
    if (version == NULL) {
        snprintf(error, error_size, "Missing version");
        return false;
    }
    if (strstr(version, "1") == NULL) {
        snprintf(error, error_size, "Unsupported protocol version");
        return false;
    }

    static char type_buf[64];
    static char id_buf[64];
    const char *type = find_json_string(body, "type");
    const char *id = find_json_string(body, "id");
    if (type == NULL) {
        snprintf(error, error_size, "Missing message type");
        return false;
    }

    size_t type_len = strcspn(type, "\"");
    if (type_len >= sizeof(type_buf)) {
        type_len = sizeof(type_buf) - 1;
    }
    memcpy(type_buf, type, type_len);
    type_buf[type_len] = '\0';

    if (id != NULL) {
        size_t id_len = strcspn(id, "\"");
        if (id_len >= sizeof(id_buf)) {
            id_len = sizeof(id_buf) - 1;
        }
        memcpy(id_buf, id, id_len);
        id_buf[id_len] = '\0';
        out->id = id_buf;
    } else {
        out->id = "0";
    }

    out->type = type_buf;
    const char *payload = strstr(body, "\"payload\"");
    out->payload_json = payload != NULL ? payload : "{}";
    return true;
}

int avar_extension_format_ping_response(char *buffer, size_t buffer_size)
{
    return snprintf(buffer,
                    buffer_size,
                    "{\"ok\":true,\"bridgeVersion\":\"%s\",\"protocolVersion\":%d}",
                    AVAR_EXTENSION_BRIDGE_VERSION,
                    AVAR_EXTENSION_PROTOCOL_VERSION);
}

int avar_extension_format_status_response(char *buffer, size_t buffer_size, bool enabled, bool connected)
{
    return snprintf(buffer,
                    buffer_size,
                    "{\"ok\":true,\"enabled\":%s,\"connected\":%s,\"bridgeVersion\":\"%s\"}",
                    enabled ? "true" : "false",
                    connected ? "true" : "false",
                    AVAR_EXTENSION_BRIDGE_VERSION);
}

int avar_extension_format_v1_response(const char *type,
                                      const char *id,
                                      bool ok,
                                      const char *payload_json,
                                      const char *error_message,
                                      char *buffer,
                                      size_t buffer_size)
{
    if (!ok) {
        return snprintf(buffer,
                        buffer_size,
                        "{\"protocol\":\"%s\",\"version\":%d,\"type\":\"%s\",\"id\":\"%s\","
                        "\"ok\":false,\"error\":\"%s\"}",
                        AVAR_EXTENSION_PROTOCOL_NAME,
                        AVAR_EXTENSION_PROTOCOL_VERSION,
                        type != NULL ? type : "error",
                        id != NULL ? id : "0",
                        error_message != NULL ? error_message : "error");
    }
    return snprintf(buffer,
                    buffer_size,
                    "{\"protocol\":\"%s\",\"version\":%d,\"type\":\"%s\",\"id\":\"%s\","
                    "\"ok\":true,\"payload\":%s}",
                    AVAR_EXTENSION_PROTOCOL_NAME,
                    AVAR_EXTENSION_PROTOCOL_VERSION,
                    type != NULL ? type : "response",
                    id != NULL ? id : "0",
                    payload_json != NULL ? payload_json : "{}");
}
