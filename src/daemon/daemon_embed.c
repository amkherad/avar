#include <daemon/daemon_embed.h>
#include <daemon/daemon_rpc.h>

#include <string.h>

static DaemonEmbedCtrlCNotify _ctrl_c_notify = NULL;

void daemon_embed_set_ctrl_c_notify(DaemonEmbedCtrlCNotify notify) {
    _ctrl_c_notify = notify;
}

void daemon_embed_on_ctrl_c(void) {
    if (_ctrl_c_notify != NULL) {
        _ctrl_c_notify();
    }
}

void daemon_embed_apply_gui_defaults(DaemonConfig *cfg, const char *pid_file_path) {
    if (cfg == NULL) {
        return;
    }

    daemon_config_apply_defaults(cfg);
    cfg->session.mode = AvarSessionModeLocal;
    cfg->session.transport = AvarTransportLocal;
    cfg->server.detach = false;
    cfg->server.http.enabled = false;
    cfg->server.https.enabled = false;
    cfg->server.pipe.enabled = false;
    cfg->server.unix_socket.enabled = false;
    strncpy(cfg->server.auto_shutdown, AVAR_DAEMON_AUTO_SHUTDOWN_NEVER,
            sizeof cfg->server.auto_shutdown);
    cfg->server.auto_shutdown[sizeof cfg->server.auto_shutdown - 1] = '\0';

    if (pid_file_path != NULL && pid_file_path[0] != '\0') {
        strncpy(cfg->server.pid_file, pid_file_path, sizeof cfg->server.pid_file);
        cfg->server.pid_file[sizeof cfg->server.pid_file - 1] = '\0';
    }
}

bool daemon_embed_rpc(const char *request_json, char **response_json_out) {
    if (!daemon_loop_is_running() || request_json == NULL || response_json_out == NULL) {
        return false;
    }
    return daemon_rpc_handle(request_json, response_json_out);
}
