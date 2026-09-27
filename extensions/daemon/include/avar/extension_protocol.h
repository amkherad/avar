#ifndef AVAR_EXTENSION_PROTOCOL_H
#define AVAR_EXTENSION_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>

#define AVAR_EXTENSION_PROTOCOL_NAME "avar.extension"
#define AVAR_EXTENSION_PROTOCOL_VERSION 1
#define AVAR_EXTENSION_BRIDGE_VERSION "0.1.0"
#define AVAR_EXTENSION_DEFAULT_PORT 18766

typedef struct avar_extension_message {
    const char *type;
    const char *id;
    const char *payload_json;
} avar_extension_message;

typedef struct avar_extension_bridge_settings {
    bool enabled;
    char daemon_url[512];
    char auth_token[256];
} avar_extension_bridge_settings;

bool avar_extension_parse_envelope(const char *body, avar_extension_message *out, char *error, size_t error_size);

int avar_extension_format_ping_response(char *buffer, size_t buffer_size);

int avar_extension_format_status_response(char *buffer, size_t buffer_size, bool enabled, bool connected);

int avar_extension_format_v1_response(const char *type,
                                      const char *id,
                                      bool ok,
                                      const char *payload_json,
                                      const char *error_message,
                                      char *buffer,
                                      size_t buffer_size);

#endif
