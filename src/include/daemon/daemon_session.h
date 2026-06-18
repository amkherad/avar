#ifndef AVAR_DAEMON_SESSION_H
#define AVAR_DAEMON_SESSION_H

#include <stdbool.h>

#include <daemon/daemon.h>
#include "utils.h"

typedef enum {
    DaemonSessionErrorNone,
    DaemonSessionErrorConfig,
    DaemonSessionErrorTransport,
    DaemonSessionErrorUnavailable,
    DaemonSessionErrorNotImplemented,
} DaemonSessionError;

/**
 * Loads daemon.session from config and prepares CLI routing.
 * Thread-safe: single-threaded CLI use only.
 */
DaemonSessionError daemon_session_init(void);

void daemon_session_deinit(void);

void daemon_session_force_local(bool force);

bool daemon_session_is_remote(void);

AvarSessionMode daemon_session_mode(void);

AvarTransportKind daemon_session_transport(void);

const DaemonConfig *daemon_session_config(void);

/**
 * Returns true when the daemon responds on the configured transport.
 */
bool daemon_session_ping(void);

/**
 * Delegates an operation to the remote daemon.
 */
DaemonSessionError daemon_session_delegate(stringa operation, stringa payload_json);

/**
 * Reconstructs argv and delegates the full CLI command to the remote daemon.
 */
int daemon_session_delegate_argv(int argc, char **argv);

/**
 * Queues a background download on the daemon when reachable; without --attached,
 * falls back to a local attached download when the daemon is unavailable.
 */
int daemon_session_download_url(const char *url, const char *queue, const char *name,
                                const char *proxy_url, bool attached);

#endif
