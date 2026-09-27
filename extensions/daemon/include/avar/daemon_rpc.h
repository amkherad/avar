#ifndef AVAR_DAEMON_RPC_H
#define AVAR_DAEMON_RPC_H

#include <stddef.h>

#include "avar/extension_protocol.h"

typedef struct avar_daemon_rpc_config {
    char daemon_url[512];
    char auth_token[256];
} avar_daemon_rpc_config;

/** POST JSON-RPC; writes response body (or empty) into out. Returns 0 on HTTP success. */
int avar_daemon_rpc_call(const avar_daemon_rpc_config *cfg,
                         const char *method,
                         const char *params_json,
                         char *out,
                         size_t out_size);

#endif
