#ifndef AVAR_BRIDGE_HANDLERS_H
#define AVAR_BRIDGE_HANDLERS_H

#include "avar/extension_protocol.h"
#include "avar/http_server.h"

typedef struct avar_bridge_state {
    avar_extension_bridge_settings settings;
    bool extension_seen;
} avar_bridge_state;

void avar_bridge_handle_request(const avar_http_request *request, int client_fd, avar_bridge_state *state);

#endif
