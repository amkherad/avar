#include "avar/bridge_handlers.h"

#include "avar/daemon_rpc.h"

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

    const char *token_key = strstr(body, "\"authToken\"");
    if (token_key != NULL) {
        const char *quote = strchr(token_key, ':');
        if (quote != NULL) {
            quote = strchr(quote, '"');
            if (quote != NULL) {
                quote++;
                size_t len = strcspn(quote, "\"");
                if (len >= sizeof(state->settings.auth_token)) {
                    len = sizeof(state->settings.auth_token) - 1;
                }
                memcpy(state->settings.auth_token, quote, len);
                state->settings.auth_token[len] = '\0';
            }
        }
    }
}

static void fill_rpc_config(const avar_bridge_state *state, avar_daemon_rpc_config *cfg)
{
    snprintf(cfg->daemon_url, sizeof(cfg->daemon_url), "%s", state->settings.daemon_url);
    snprintf(cfg->auth_token, sizeof(cfg->auth_token), "%s", state->settings.auth_token);
    if (cfg->daemon_url[0] == '\0') {
        snprintf(cfg->daemon_url, sizeof(cfg->daemon_url), "http://127.0.0.1:8000");
    }
}

static bool extract_json_string(const char *body, const char *key, char *out, size_t out_size)
{
    if (body == NULL || key == NULL || out == NULL || out_size == 0) {
        return false;
    }
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(body, pattern);
    if (pos == NULL) {
        return false;
    }
    pos = strchr(pos + strlen(pattern), '"');
    if (pos == NULL) {
        return false;
    }
    pos++;
    size_t len = strcspn(pos, "\"");
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, pos, len);
    out[len] = '\0';
    return out[0] != '\0';
}

static void send_v1_ok(const avar_extension_message *message,
                       const char *type,
                       const char *payload_json,
                       char *response,
                       size_t response_size,
                       int client_fd)
{
    avar_extension_format_v1_response(type, message->id, true, payload_json, NULL, response, response_size);
    avar_http_send_json(client_fd, 200, response);
}

static void send_v1_error(const avar_extension_message *message,
                          const char *type,
                          const char *error_message,
                          char *response,
                          size_t response_size,
                          int client_fd,
                          int status)
{
    avar_extension_format_v1_response(type, message->id, false, "{}", error_message, response, response_size);
    avar_http_send_json(client_fd, status, response);
}

static void handle_v1_message(const avar_extension_message *message, int client_fd, avar_bridge_state *state)
{
    char response[4096];
    char rpc_out[8192];
    avar_daemon_rpc_config rpc_cfg;
    fill_rpc_config(state, &rpc_cfg);

    if (strcmp(message->type, "ping") == 0) {
        state->extension_seen = true;
        char payload[128];
        snprintf(payload,
                 sizeof(payload),
                 "{\"bridgeVersion\":\"%s\",\"protocolVersion\":%d}",
                 AVAR_EXTENSION_BRIDGE_VERSION,
                 AVAR_EXTENSION_PROTOCOL_VERSION);
        send_v1_ok(message, "ping", payload, response, sizeof(response), client_fd);
        return;
    }

    if (strcmp(message->type, "status") == 0) {
        char payload[256];
        snprintf(payload,
                 sizeof(payload),
                 "{\"enabled\":%s,\"bridgeVersion\":\"%s\"}",
                 state->settings.enabled ? "true" : "false",
                 AVAR_EXTENSION_BRIDGE_VERSION);
        send_v1_ok(message, "status", payload, response, sizeof(response), client_fd);
        return;
    }

    if (strcmp(message->type, "download.add") == 0) {
        char url[2048];
        if (!extract_json_string(message->payload_json, "url", url, sizeof(url))) {
            send_v1_error(message, "download.add", "Missing url", response, sizeof(response), client_fd, 400);
            return;
        }
        char params[2300];
        snprintf(params, sizeof(params), "{\"url\":\"%s\",\"attached\":false}", url);
        if (avar_daemon_rpc_call(&rpc_cfg, "download.add", params, rpc_out, sizeof(rpc_out)) != 0) {
            send_v1_error(message, "download.add", "Daemon RPC failed", response, sizeof(response), client_fd, 502);
            return;
        }
        send_v1_ok(message, "download.add", "{}", response, sizeof(response), client_fd);
        return;
    }

    if (strcmp(message->type, "url.probe") == 0) {
        char url[2048];
        if (!extract_json_string(message->payload_json, "url", url, sizeof(url))) {
            send_v1_error(message, "url.probe", "Missing url", response, sizeof(response), client_fd, 400);
            return;
        }
        char params[2300];
        snprintf(params, sizeof(params), "{\"url\":\"%s\"}", url);
        if (avar_daemon_rpc_call(&rpc_cfg, "download.probe", params, rpc_out, sizeof(rpc_out)) != 0) {
            send_v1_error(message, "url.probe", "Daemon RPC failed", response, sizeof(response), client_fd, 502);
            return;
        }
        const char *result = strstr(rpc_out, "\"result\"");
        char payload[4096];
        if (result != NULL) {
            snprintf(payload, sizeof(payload), "%s", result);
        } else {
            snprintf(payload, sizeof(payload), "{}");
        }
        send_v1_ok(message, "url.probe", "{}", response, sizeof(response), client_fd);
        return;
    }

    if (strcmp(message->type, "queue.list") == 0) {
        if (avar_daemon_rpc_call(&rpc_cfg, "queue.list", "{}", rpc_out, sizeof(rpc_out)) != 0) {
            send_v1_error(message, "queue.list", "Daemon RPC failed", response, sizeof(response), client_fd, 502);
            return;
        }
        send_v1_ok(message, "queue.list", "{}", response, sizeof(response), client_fd);
        return;
    }

    if (strcmp(message->type, "queue.start") == 0 || strcmp(message->type, "queue.stop") == 0) {
        char queue_id[128];
        if (!extract_json_string(message->payload_json, "id", queue_id, sizeof(queue_id))) {
            send_v1_error(message, message->type, "Missing queue id", response, sizeof(response), client_fd, 400);
            return;
        }
        const char *method = strcmp(message->type, "queue.start") == 0 ? "queue.start" : "queue.stop";
        char params[200];
        snprintf(params, sizeof(params), "{\"id\":\"%s\"}", queue_id);
        if (avar_daemon_rpc_call(&rpc_cfg, method, params, rpc_out, sizeof(rpc_out)) != 0) {
            send_v1_error(message, message->type, "Daemon RPC failed", response, sizeof(response), client_fd, 502);
            return;
        }
        send_v1_ok(message, message->type, "{}", response, sizeof(response), client_fd);
        return;
    }

    send_v1_error(message, message->type, "Not implemented", response, sizeof(response), client_fd, 501);
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

        handle_v1_message(&message, client_fd, state);
        return;
    }

    avar_http_send_json(client_fd, 404, "{\"ok\":false,\"error\":\"Not found\"}");
}
