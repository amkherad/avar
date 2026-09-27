#ifndef AVAR_DAEMON_EMBED_H
#define AVAR_DAEMON_EMBED_H

#include <stdbool.h>

#include <daemon/daemon.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configures an in-process daemon for GUI embedding: local session, in-memory RPC
 * transport, no HTTP/pipe/unix listeners, no auto-shutdown on idle.
 * pid_file_path must be a writable unique path for this GUI instance.
 */
void daemon_embed_apply_gui_defaults(DaemonConfig *cfg, const char *pid_file_path);

/** True while daemon_start's main loop is active in this process. */
bool daemon_loop_is_running(void);

/**
 * Handles a JSON-RPC request in-process (AvarTransportLocal).
 * Caller must free(*response_json_out). Returns false when the daemon loop is not running.
 */
bool daemon_embed_rpc(const char *request_json, char **response_json_out);

/** Optional async-signal-safe hook invoked from the daemon SIGINT handler (GUI quit). */
typedef void (*DaemonEmbedCtrlCNotify)(void);

void daemon_embed_set_ctrl_c_notify(DaemonEmbedCtrlCNotify notify);

void daemon_embed_on_ctrl_c(void);

#ifdef __cplusplus
}
#endif

#endif
