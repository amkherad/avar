#include <cJSON.h>

#include <daemon_session.h>
#include <daemon_rpc.h>
#include <daemon_transport.h>
#include <download.h>
#include <logger.h>

#include <string.h>

static DaemonConfig _session_cfg;
static bool _session_ready = false;
static bool _force_local = false;

DaemonSessionError daemon_session_init(void) {
    if (!daemon_config_load(&_session_cfg)) {
        return DaemonSessionErrorConfig;
    }
    _session_ready = true;
    LOG_DEBUG("Daemon session: mode=%s transport=%s",
              _session_cfg.session.mode == AvarSessionModeRemote ? "remote" : "local",
              _session_cfg.session.transport == AvarTransportPipe    ? "pipe"
              : _session_cfg.session.transport == AvarTransportUnix ? "unix"
              : _session_cfg.session.transport == AvarTransportHttp  ? "http"
                                                                    : "local");
    return DaemonSessionErrorNone;
}

void daemon_session_deinit(void) {
    _session_ready = false;
}

void daemon_session_force_local(const bool force) {
    _force_local = force;
}

bool daemon_session_is_remote(void) {
    return !_force_local && _session_ready && _session_cfg.session.mode == AvarSessionModeRemote;
}

AvarSessionMode daemon_session_mode(void) {
    return _session_ready ? _session_cfg.session.mode : AvarSessionModeLocal;
}

AvarTransportKind daemon_session_transport(void) {
    return _session_ready ? _session_cfg.session.transport : AvarTransportLocal;
}

const DaemonConfig *daemon_session_config(void) {
    return _session_ready ? &_session_cfg : NULL;
}

bool daemon_session_ping(void) {
    if (!_session_ready) {
        return false;
    }

    return daemon_transport_ping_any(&_session_cfg);
}

DaemonSessionError daemon_session_delegate(const stringa operation, const stringa payload_json) {
    if (!_session_ready) {
        return DaemonSessionErrorConfig;
    }

    if (_session_cfg.session.mode != AvarSessionModeRemote) {
        return DaemonSessionErrorNone;
    }

    char *result = NULL;
    const bool ok = daemon_rpc_call(_session_cfg.session.transport, &_session_cfg, operation,
                                    payload_json, &result, NULL);
    free(result);

    if (!ok) {
        LOG_ERROR("Daemon delegation failed for operation '%s'", operation);
        return DaemonSessionErrorTransport;
    }

    return DaemonSessionErrorNone;
}

int daemon_session_delegate_argv(const int argc, char **argv) {
    if (!_session_ready) {
        return EXIT_FAILURE;
    }

    if (_session_cfg.session.mode != AvarSessionModeRemote) {
        return EXIT_FAILURE;
    }

    return daemon_rpc_delegate_argv(argc, argv);
}

int daemon_session_download_url(const char *url, const char *queue, const char *name,
                                const bool attached) {
    if (url == NULL || !is_valid_http_url(url)) {
        LOG_ERROR("Invalid download URL");
        return EXIT_FAILURE;
    }

    if (attached) {
        return transient_download(url, queue, name, true);
    }

    if (daemon_session_ping()) {
        cJSON *params = cJSON_CreateObject();
        if (params != NULL) {
            cJSON_AddStringToObject(params, "url", url);
            cJSON_AddBoolToObject(params, "attached", false);
            if (queue != NULL) {
                cJSON_AddStringToObject(params, "queue", queue);
            }

            char *params_str = cJSON_PrintUnformatted(params);
            cJSON_Delete(params);
            if (params_str != NULL) {
                char *result = NULL;
                int exit_code = EXIT_FAILURE;
                if (daemon_rpc_call(_session_cfg.session.transport, &_session_cfg, "download.add",
                                    params_str, &result, &exit_code)) {
                    free(params_str);
                    free(result);
                    return exit_code;
                }
                free(params_str);
                free(result);
            }
        }
    }

    LOG_INFO("Daemon unavailable; running attached download locally");
    return transient_download(url, queue, name, true);
}
