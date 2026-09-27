#include "avar/bridge_handlers.h"

#include <stdio.h>
#include <string.h>

static void apply_settings_patch(avar_bridge_state *state, const char *body)
{
    if (body == NULL) {
        return;
    }
    if (strstr(body, "\"enabled\":true") != NULL || strstr(body, "\"enabled\": true") != NULL) {
        state->settings.enabled = true;
    }
    if (strstr(body, "\"enabled\":false") != NULL || strstr(body, "\"enabled\": false") != NULL) {
        state->settings.enabled = false;
    }

    const char *daemon_key = strstr(body, "\"daemonUrl\"");
    if (daemon_key != NULL) {
        const char *quote = strchr(daemon_key, ':');
        if (quote != NULL) {
            quote = strchr(quote, '"');
            if (quote != NULL) {
                quote++;
                size_t len = strcspn(quote, "\"");
                if (len >= sizeof(state->settings.daemon_url)) {
                    len = sizeof(state->settings.daemon_url) - 1;
                }
                memcpy(state->settings.daemon_url, quote, len);
                state->settings.daemon_url[len] = '\0';
            }
        }
    }
}

void avar_bridge_handle_request(const avar_http_request *request, int client_fd, avar_bridge_state *state)
{
    char response[2048];

    if (strcmp(request->path, "/v1/ping") == 0 && strcmp(request->method, "GET") == 0) {
        avar_extension_format_ping_response(response, sizeof(response));
        avar_http_send_json(client_fd, 200, response);
        return;
    }

    if (strcmp(request->path, "/extension/status") == 0 && strcmp(request->method, "GET") == 0) {
        avar_extension_format_status_response(response,
                                              sizeof(response),
                                              state->settings.enabled,
                                              state->extension_seen);
        avar_http_send_json(client_fd, 200, response);
        return;
    }

    if (strcmp(request->path, "/extension/settings") == 0 && strcmp(request->method, "POST") == 0) {
        apply_settings_patch(state, request->body);
        avar_http_send_json(client_fd, 200, "{\"ok\":true}");
        return;
    }

    if (strcmp(request->path, "/v1") == 0 && strcmp(request->method, "POST") == 0) {
        if (!state->settings.enabled) {
            avar_http_send_json(client_fd,
                                503,
                                "{\"ok\":false,\"error\":\"Browser extension bridge is disabled in Avar settings.\"}");
            return;
        }

        avar_extension_message message;
        char error[128];
        if (!avar_extension_parse_envelope(request->body, &message, error, sizeof(error))) {
            avar_http_send_json(client_fd, 400, "{\"ok\":false,\"error\":\"Invalid envelope\"}");
            return;
        }

        if (strcmp(message.type, "ping") == 0) {
            state->extension_seen = true;
            avar_extension_format_v1_response("ping", message.id, true, "{}", NULL, response, sizeof(response));
            avar_http_send_json(client_fd, 200, response);
            return;
        }

        if (strcmp(message.type, "status") == 0) {
            char payload[256];
            snprintf(payload,
                     sizeof(payload),
                     "{\"enabled\":%s,\"bridgeVersion\":\"%s\"}",
                     state->settings.enabled ? "true" : "false",
                     AVAR_EXTENSION_BRIDGE_VERSION);
            avar_extension_format_v1_response("status", message.id, true, payload, NULL, response, sizeof(response));
            avar_http_send_json(client_fd, 200, response);
            return;
        }

        avar_http_send_json(client_fd, 501, "{\"ok\":false,\"error\":\"Not implemented in standalone daemon yet\"}");
        return;
    }

    avar_http_send_json(client_fd, 404, "{\"ok\":false,\"error\":\"Not found\"}");
}
