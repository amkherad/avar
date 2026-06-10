#ifndef AVAR_DAEMON_TRANSPORT_H
#define AVAR_DAEMON_TRANSPORT_H

#include <stdbool.h>

#include <daemon/daemon.h>

typedef struct DaemonTransport DaemonTransport;

typedef struct {
    bool (*start)(DaemonTransport *self, const DaemonConfig *cfg);
    void (*stop)(DaemonTransport *self);
    bool (*ping)(DaemonTransport *self);
    void (*destroy)(DaemonTransport *self);
} DaemonTransportVTable;

struct DaemonTransport {
    const DaemonTransportVTable *vtable;
    void *context;
};

DaemonTransport *daemon_transport_create(AvarTransportKind kind);

DaemonTransport *daemon_transport_create_https(void);

bool daemon_transport_start(DaemonTransport *transport, const DaemonConfig *cfg);

void daemon_transport_stop(DaemonTransport *transport);

bool daemon_transport_ping(DaemonTransport *transport);

void daemon_transport_destroy(DaemonTransport *transport);

bool daemon_transport_ping_remote(AvarTransportKind kind, const DaemonConfig *cfg);

bool daemon_transport_ping_any(const DaemonConfig *cfg);

bool daemon_transport_ping_any_timeout(const DaemonConfig *cfg, unsigned timeout_ms);

void daemon_transport_poll(DaemonTransport *transport, unsigned timeout_ms);

bool daemon_transport_rpc_request(AvarTransportKind kind, const DaemonConfig *cfg,
                                  const char *request_json, char **response_json_out);

bool daemon_transport_rpc_request_any(const DaemonConfig *cfg, const char *request_json,
                                      char **response_json_out);

#endif
