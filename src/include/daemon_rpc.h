#ifndef AVAR_DAEMON_RPC_H
#define AVAR_DAEMON_RPC_H

#include <stdbool.h>
#include <stddef.h>

#include "daemon.h"

struct mg_http_message;

#define AVAR_DAEMON_RPC_FRAME_MAX (256U * 1024U)

void daemon_rpc_init(void);

void daemon_rpc_set_auth_token(const char *token);

void daemon_rpc_shutdown(void);

/**
 * Handles a JSON-RPC 2.0 request string and writes a newly allocated response.
 * Caller must free(*response_json_out).
 */
bool daemon_rpc_handle(const char *request_json, char **response_json_out);

/**
 * Returns true when the HTTP Authorization header matches the configured token.
 * When no token is configured, all requests are allowed.
 */
bool daemon_rpc_check_http_auth(struct mg_http_message *hm);

/**
 * Dispatches an HTTP request URI/body to the shared RPC layer.
 * Writes a newly allocated response body. Caller must free(*body_out).
 */
bool daemon_rpc_handle_http(const char *uri, const char *body, size_t body_len,
                            char **body_out, int *status_out);

/**
 * Invokes an RPC method on the daemon via the configured transport.
 * When result_json_out is non-NULL, writes the result field (caller must free).
 */
bool daemon_rpc_call(AvarTransportKind transport, const DaemonConfig *cfg,
                     const char *method, const char *params_json,
                     char **result_json_out, int *exit_code_out);

/**
 * Serializes argv and delegates to cli.exec on the remote daemon.
 */
int daemon_rpc_delegate_argv(int argc, char **argv);

void daemon_rpc_log_append(const char *line);

bool daemon_rpc_logs_fetch(bool follow, unsigned max_lines, char **text_out);

#endif
